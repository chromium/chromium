// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"

namespace {

#if BUILDFLAG(IS_ANDROID)
// Set the render host waiting time to 5s on Android, that's the same
// as an "Application Not Responding" timeout.
const int64_t kTimerDelaySeconds = 5;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
// linux-chromeos-dbg is failing to destroy the profile in under 1 second
const int64_t kTimerDelaySeconds = 2;
#else
const int64_t kTimerDelaySeconds = 1;
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProfileDestructionType {
  kImmediately = 0,
  kDelayed = 1,
  kDelayedAndCrashed = 2,
  kMaxValue = kDelayedAndCrashed,
};

using DestroyerSet = std::set<ProfileDestroyer*>;
DestroyerSet& PendingDestroyers() {
  static base::NoDestructor<DestroyerSet> instance;
  return *instance;
}

// Given a `profile`, returns the set of profiles that needs to be deleted
// first.
std::vector<Profile*> GetDependentProfiles(Profile* profile) {
  if (profile->IsOffTheRecord())
    return {};
  return profile->GetAllOffTheRecordProfiles();
}

}  // namespace

class OTRProfileDestroyer : public ProfileDestroyer {
 public:
  OTRProfileDestroyer(Profile* profile, base::TimeDelta timeout)
      : ProfileDestroyer(profile, timeout), profile_(profile->GetWeakPtr()) {}
  ~OTRProfileDestroyer() override = default;

 protected:
  Profile* GetProfile() override { return profile_.get(); }

  void DoDestroyUnderlyingProfile() override {
    Profile* profile = profile_.get();
    if (!profile)
      return;
    ProfileDestroyer::DestroyOffTheRecordProfileNow(profile);
  }

  void RetryDestroyUnderlyingProfile() override {
    Profile* profile = profile_.get();
    if (!profile)
      return;
    ProfileDestroyer::DestroyOTRProfileWhenAppropriateWithTimeout(profile,
                                                                  timeout());
  }

 private:
  base::WeakPtr<Profile> profile_;
};

class OriginalProfileDestroyer : public ProfileDestroyer {
 public:
  OriginalProfileDestroyer(std::unique_ptr<Profile> profile,
                           base::TimeDelta timeout)
      : ProfileDestroyer(profile.get(), timeout),
        profile_(std::move(profile)) {}
  ~OriginalProfileDestroyer() override = default;

 protected:
  Profile* GetProfile() override { return profile_.get(); }

  void DoDestroyUnderlyingProfile() override {
    DCHECK(profile_);
    ProfileDestroyer::DestroyOriginalProfileNow(std::move(profile_));
  }

  void RetryDestroyUnderlyingProfile() override {
    DCHECK(profile_);
    ProfileDestroyer::DestroyOriginalProfileWhenAppropriateWithTimeout(
        std::move(profile_), timeout());
  }

 private:
  std::unique_ptr<Profile> profile_;
};

// static
void ProfileDestroyer::DestroyOriginalProfileWhenAppropriate(
    std::unique_ptr<Profile> profile) {
  DestroyOriginalProfileWhenAppropriateWithTimeout(
      std::move(profile), base::Seconds(kTimerDelaySeconds));
}

void ProfileDestroyer::DestroyOriginalProfileWhenAppropriateWithTimeout(
    std::unique_ptr<Profile> profile,
    base::TimeDelta timeout) {
  DCHECK(profile);
  DCHECK_EQ(profile.get(), profile->GetOriginalProfile());
  DCHECK(!GetPendingDestroyerForProfile(profile.get()));

  TRACE_EVENT(
      "shutdown",
      "ProfileDestroyer::DestroyOriginalProfileWhenAppropriateWithTimeout",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_profile_destroyer();
        proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile.get()));
        proto->set_is_off_the_record(profile->IsOffTheRecord());
      });

  profile->MaybeSendDestroyedNotification();

  HostSet profile_hosts;
  GetHostsForProfile(&profile_hosts, profile.get());
  for (Profile* otr_profile : GetDependentProfiles(profile.get())) {
    GetHostsForProfile(&profile_hosts, otr_profile);
  }

  OriginalProfileDestroyer* profile_destroyer =
      new OriginalProfileDestroyer(std::move(profile), timeout);
  profile_destroyer->Start(profile_hosts);
}

void ProfileDestroyer::DestroyOTRProfileWhenAppropriate(Profile* profile) {
  DestroyOTRProfileWhenAppropriateWithTimeout(
      profile, base::Seconds(kTimerDelaySeconds));
}

void ProfileDestroyer::DestroyOTRProfileImmediately(Profile* profile) {
  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyOTRProfileImmediately",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
                proto->set_is_off_the_record(profile->IsOffTheRecord());
              });

  ProfileDestroyer* pending_destroyer = GetPendingDestroyerForProfile(profile);
  if (pending_destroyer) {
    pending_destroyer->Timeout();
    return;
  }

  // Passing zero timeout forces the destruction of the profile synchronously.
  DestroyOTRProfileWhenAppropriateWithTimeout(profile, base::TimeDelta());
}

void ProfileDestroyer::DestroyOTRProfileWhenAppropriateWithTimeout(
    Profile* profile,
    base::TimeDelta timeout) {
  DCHECK(profile);
  DCHECK_NE(profile, profile->GetOriginalProfile());

  if (GetPendingDestroyerForProfile(profile))
    return;

  TRACE_EVENT("shutdown",
              "ProfileDestroyer::DestroyOTRProfileWhenAppropriateWithTimeout",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
                proto->set_is_off_the_record(profile->IsOffTheRecord());
              });

  profile->MaybeSendDestroyedNotification();

  HostSet profile_hosts;
  GetHostsForProfile(&profile_hosts, profile);
  OTRProfileDestroyer* profile_destroyer =
      new OTRProfileDestroyer(profile, timeout);
  profile_destroyer->Start(profile_hosts);
}

// static
void ProfileDestroyer::DestroyPendingProfilesForShutdown() {
  while (!PendingDestroyers().empty()) {
    TRACE_EVENT("shutdown",
                "ProfileDestroyer::DestroyPendingProfilesForShutdown");
    ProfileDestroyer* destroyer = *(PendingDestroyers().begin());
    // Destroys `destroyer`and removes it from `PendingDestroyers()`:
    destroyer->Timeout();
  }
}

// static
void ProfileDestroyer::DestroyOffTheRecordProfileNow(Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  TRACE_EVENT(
      "shutdown", "ProfileDestroyer::DestroyOffTheRecordProfileNow",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_profile_destroyer();
        proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
        std::stringstream otr_id;
        otr_id << profile->GetOTRProfileID();
        proto->set_otr_profile_id(otr_id.str());
      });

  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile(profile);
  UMA_HISTOGRAM_ENUMERATION("Profile.Destroyer.OffTheRecord",
                            ProfileDestructionType::kImmediately);
}

// static
void ProfileDestroyer::DestroyOriginalProfileNow(
    std::unique_ptr<Profile> profile) {
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());
  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyOriginalProfileNow",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(
                    reinterpret_cast<uint64_t>(profile.get()));
              });

  // With DestroyProfileOnBrowserClose and --single-process, we need to clean up
  // the RPH first. Single-process mode does not support multiple Profiles, so
  // this will not interfere with other Profiles.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose) &&
      content::RenderProcessHost::run_renderer_in_process()) {
    HostSet rph;
    GetHostsForProfile(&rph, profile.get(), /*include_spare_rph=*/true);
    if (!rph.empty()) {
      content::RenderProcessHost::ShutDownInProcessRenderer();
    }
  }

#if DCHECK_IS_ON()
  // Save the raw pointers of profile and dependent profile for DCHECKing on
  // later.
  void* profile_ptr = profile.get();
  std::vector<Profile*> dependent_profile = GetDependentProfiles(profile.get());
#endif  // DCHECK_IS_ON()

  profile.reset();

#if DCHECK_IS_ON()
  // Count the number of hosts that have dangling pointers to the freed Profile
  // and off-the-record Profile.
  HostSet dangling_hosts;
  HostSet dangling_hosts_for_otr;
  GetHostsForProfile(&dangling_hosts, profile_ptr);
  for (Profile* otr : dependent_profile) {
    GetHostsForProfile(&dangling_hosts_for_otr, otr);
  }
  const size_t profile_hosts_count = dangling_hosts.size();
  const size_t off_the_record_profile_hosts_count =
      dangling_hosts_for_otr.size();
  base::debug::Alias(&profile_hosts_count);
  base::debug::Alias(&off_the_record_profile_hosts_count);

  // |profile| is not off-the-record, so if |profile_hosts| is not empty then
  // something has leaked a RenderProcessHost, and needs fixing.
  //
  // The exception is that RenderProcessHostImpl::Release() avoids destroying
  // RenderProcessHosts in --single-process mode, to avoid race conditions.
  if (!content::RenderProcessHost::run_renderer_in_process()) {
    DCHECK_EQ(profile_hosts_count, 0u);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // ChromeOS' system profile can be outlived by its off-the-record profile
    // (see https://crbug.com/828479).
    DCHECK_EQ(off_the_record_profile_hosts_count, 0u);
#endif
  }
#endif  // DCHECK_IS_ON()
}

ProfileDestroyer::ProfileDestroyer(Profile* profile, base::TimeDelta timeout)
    : timeout_(timeout), profile_ptr_(reinterpret_cast<uint64_t>(profile)) {
  PendingDestroyers().insert(this);
}

void ProfileDestroyer::Start(const HostSet& hosts) {
  TRACE_EVENT("shutdown", "ProfileDestroyer::ProfileDestroyer",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(profile_ptr_);
                proto->set_host_count_at_creation(hosts.size());
              });

  for (auto* host : hosts)
    observations_.AddObservation(host);

  if (!observations_.IsObservingAnySource()) {
    // No renderer process to wait for. Destroy profile now.
    Timeout();
    return;
  }

  if (timeout_.is_zero()) {
    // Zero timeout means synchronous destruction of the underlying profile.
    Timeout();
    return;
  }

  // We don't want to wait for RenderProcessHost to be destroyed longer than
  // timeout.
  timer_.Start(FROM_HERE, timeout_,
               base::BindOnce(&ProfileDestroyer::Timeout,
                              weak_ptr_factory_.GetWeakPtr()));
}

ProfileDestroyer::~ProfileDestroyer() {
  TRACE_EVENT("shutdown", "ProfileDestroyer::~ProfileDestroyer",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(profile_ptr_);
                proto->set_host_count_at_destruction(
                    observations_.GetSourcesCount());
              });
  // Don't wait for pending registrations, if any, these hosts are buggy.
  // Note: this can happen, but if so, it's better to crash here than wait
  // for the host to dereference a deleted Profile. http://crbug.com/248625
  UMA_HISTOGRAM_ENUMERATION("Profile.Destroyer.OffTheRecord",
                            observations_.IsObservingAnySource()
                                ? ProfileDestructionType::kDelayedAndCrashed
                                : ProfileDestructionType::kDelayed);
  // If this is crashing, a renderer process host is not destroyed fast enough
  // during shutdown of the browser and deletion of the profile.
  CHECK(!observations_.IsObservingAnySource())
      << "Some render process hosts were not destroyed early enough!";
  auto iter = PendingDestroyers().find(this);
  CHECK(iter != PendingDestroyers().end(), base::NotFatalUntil::M130);
  PendingDestroyers().erase(iter);
}

void ProfileDestroyer::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  TRACE_EVENT(
      "shutdown", "ProfileDestroyer::RenderProcessHostDestroyed",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_profile_destroyer();
        proto->set_profile_ptr(profile_ptr_);
        proto->set_render_process_host_ptr(reinterpret_cast<uint64_t>(host));
      });
  observations_.RemoveObservation(host);
  if (observations_.IsObservingAnySource())
    return;

  // This instance is no more observing any RenderProcessHost. They are all
  // deleted. It is time to retry deleting the profile.
  //
  // Note that this can loop several time, because some new RenderProcessHost
  // might have been added in the meantime.
  // TODO(arthursonzogni): Consider adding some TTL logic, because this might
  // (unlikely) retry for a long time.
  //
  // Delay the retry one step further in case other observers need to look at
  // the profile attached to the host.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProfileDestroyer::Retry, weak_ptr_factory_.GetWeakPtr()));
}

// static
void ProfileDestroyer::GetHostsForProfile(HostSet* out,
                                          void* profile_ptr,
                                          bool include_spare_rph) {
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    DCHECK(render_process_host);

    if (render_process_host->GetBrowserContext() != profile_ptr)
      continue;

    // Ignore the spare RenderProcessHost.
    if (render_process_host->IsSpare() && !include_spare_rph) {
      continue;
    }

    TRACE_EVENT(
        "shutdown", "ProfileDestroyer::GetHostsForProfile",
        [&](perfetto::EventContext ctx) {
          auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                            ->set_chrome_profile_destroyer();
          proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile_ptr));
          proto->set_render_process_host_ptr(
              reinterpret_cast<uint64_t>(render_process_host));
        });
    out->insert(render_process_host);
  }
}

void ProfileDestroyer::Timeout() {
  DCHECK(!is_prepared_for_destruction_);
  is_prepared_for_destruction_ = true;

  // Destroying the profile destroys remote hosts, so it is important to keep
  // |this| alive while the underlying profile is destroyed as otherwise the
  // destructor will crash on line CHECK(!observations_.IsObservingAnySource()).
  DoDestroyUnderlyingProfile();

  delete this;  // Final state.
}

void ProfileDestroyer::ProfileDestroyer::Retry() {
  DCHECK(!is_prepared_for_destruction_);
  is_prepared_for_destruction_ = true;

  RetryDestroyUnderlyingProfile();

  delete this;  // Final state.
}

// static.
ProfileDestroyer* ProfileDestroyer::GetPendingDestroyerForProfile(
    const Profile* profile) {
  for (ProfileDestroyer* destroyer : PendingDestroyers()) {
    if (destroyer->GetProfile() == profile &&
        !destroyer->is_prepared_for_destruction()) {
      return destroyer;
    }
  }
  return nullptr;
}

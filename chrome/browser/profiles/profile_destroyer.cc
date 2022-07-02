// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

}  // namespace

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  if (!profile)  // profile might have been reset in ResetPendingDestroyers();
    return;

  // We allow multiple calls to `DestroyProfileWhenAppropriate` for the same
  // Profile. A new request replaces the previous one, so that there are never
  // more than one ProfileDestroyer for the same profile.
  // See https://crbug.com/1337388#c12
  ResetPendingDestroyers(profile);

  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyProfileWhenAppropriate",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
                proto->set_is_off_the_record(profile->IsOffTheRecord());
              });
  profile->MaybeSendDestroyedNotification();

  // Profiles may have DestroyProfileWhenAppropriate() called before their
  // RenderProcessHosts are gone. When this happens, we need to defer their
  // deletion.
  //
  // TODO(arthursonzogni): Explore adding a ScopedProfileKeepAlive in
  // RenderProcessHost. This would remove the need of a ProfileDestroyer waiting
  // for RenderProcessHost deletion. It will make Chrome more stable and fix
  // some UAF bugs.
  //
  // Note: The original profile waits for both its own RenderProcessHost and its
  // OffTheRecord Profiles's RenderProcessHosts. It is slightly safer. OTR
  // profiles holds a ScopedProfileKeepAlive on their parent and are deleted
  // first, so this seems unnecessary, but ScopedProfileKeepAlive logic is
  // ignored during shutdown and by the System Profile do not either.
  HostSet profile_hosts;
  GetHostsForProfile(&profile_hosts, profile);
  for (Profile* otr_profile : profile->GetAllOffTheRecordProfiles()) {
    GetHostsForProfile(&profile_hosts, otr_profile);
  }

  if (!profile_hosts.empty()) {
    // The instance will destroy itself once all (non-spare) render process
    // hosts referring to it are properly terminated. This happens in the two
    // "final" state: Retry() and Timeout().
    new ProfileDestroyer(profile, profile_hosts);
    return;
  }

  DestroyProfileNow(profile);
}

// static
void ProfileDestroyer::DestroyPendingProfilesForShutdown() {
  while (!PendingDestroyers().empty()) {
    ProfileDestroyer* destroyer = *(PendingDestroyers().begin());
    // Destroys `destroyer`and removes it from `PendingDestroyers()`:
    destroyer->Timeout();
  }
}

// static
void ProfileDestroyer::DestroyOffTheRecordProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  TRACE_EVENT(
      "shutdown", "ProfileDestroyer::DestroyOffTheRecordProfileNow",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_profile_destroyer();
        proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
        proto->set_otr_profile_id(profile->GetOTRProfileID().ToString());
      });

  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile(profile);
  UMA_HISTOGRAM_ENUMERATION("Profile.Destroyer.OffTheRecord",
                            ProfileDestructionType::kImmediately);
}

// static
void ProfileDestroyer::DestroyProfileNow(Profile* const profile) {
  if (!profile)  // profile might have been reset in ResetPendingDestroyers();
    return;

  // Make sure we don't delete the same profile twice, otherwise this would have
  // been a UAF.
  ResetPendingDestroyers(profile);

  if (profile->IsOffTheRecord())
    DestroyOffTheRecordProfileNow(profile);
  else
    DestroyOriginalProfileNow(profile);
}

// static
void ProfileDestroyer::DestroyOriginalProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());
  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyOriginalProfileNow",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
              });

  // With DestroyProfileOnBrowserClose and --single-process, we need to clean up
  // the RPH first. Single-process mode does not support multiple Profiles, so
  // this will not interfere with other Profiles.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose) &&
      content::RenderProcessHost::run_renderer_in_process()) {
    HostSet rph;
    GetHostsForProfile(&rph, profile, /*include_spare_rph=*/true);
    if (!rph.empty()) {
      content::RenderProcessHost::ShutDownInProcessRenderer();
    }
  }

#if DCHECK_IS_ON()
  // Save the raw pointers of profile and off-the-record profile for DCHECKing
  // on later.
  void* profile_ptr = profile;
  std::vector<Profile*> otr_profiles = profile->GetAllOffTheRecordProfiles();
#endif  // DCHECK_IS_ON()

  delete profile;

#if DCHECK_IS_ON()
  // Count the number of hosts that have dangling pointers to the freed Profile
  // and off-the-record Profile.
  HostSet dangling_hosts;
  HostSet dangling_hosts_for_otr;
  GetHostsForProfile(&dangling_hosts, profile_ptr);
  for (Profile* otr : otr_profiles) {
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

// static
void ProfileDestroyer::ResetPendingDestroyers(Profile* const profile) {
  for (auto* i : PendingDestroyers()) {
    if (i->profile_ == profile) {
      i->profile_ = nullptr;
    }
  }
}

ProfileDestroyer::ProfileDestroyer(Profile* const profile, const HostSet& hosts)
    : profile_(profile), profile_ptr_(reinterpret_cast<uint64_t>(profile)) {
  TRACE_EVENT("shutdown", "ProfileDestroyer::ProfileDestroyer",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(profile_ptr_);
                proto->set_host_count_at_creation(hosts.size());
              });
  DCHECK(!hosts.empty());
  PendingDestroyers().insert(this);
  for (auto* host : hosts)
    observations_.AddObservation(host);
  DCHECK(observations_.IsObservingAnySource());

  // We don't want to wait for RenderProcessHost to be destroyed longer than
  // kTimerDelaySeconds.
  timer_.Start(FROM_HERE, base::Seconds(kTimerDelaySeconds),
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
  DCHECK(!profile_);

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
  DCHECK(iter != PendingDestroyers().end());
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProfileDestroyer::Retry, weak_ptr_factory_.GetWeakPtr()));
}

void ProfileDestroyer::Timeout() {
  DestroyProfileNow(profile_);
  delete this;  // Final state.
}

void ProfileDestroyer::Retry() {
  DestroyProfileWhenAppropriate(profile_);
  delete this;  // Final state.
}

// static
void ProfileDestroyer::GetHostsForProfile(HostSet* out,
                                          void* const profile_ptr,
                                          bool include_spare_rph) {
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    DCHECK(render_process_host);

    if (render_process_host->GetBrowserContext() != profile_ptr)
      continue;

    // Ignore the spare RenderProcessHost.
    if (render_process_host->HostHasNotBeenUsed() && !include_spare_rph)
      continue;

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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"

namespace {

#if defined(OS_ANDROID)
// Set the render host waiting time to 5s on Android, that's the same
// as an "Application Not Responding" timeout.
const int64_t kTimerDelaySeconds = 5;
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

}  // namespace

ProfileDestroyer::DestroyerSet* ProfileDestroyer::pending_destroyers_ = nullptr;

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyProfileWhenAppropriate",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
                proto->set_is_off_the_record(profile->IsOffTheRecord());
              });

  DCHECK(profile);
  profile->MaybeSendDestroyedNotification();

  if (!profile->IsOffTheRecord()) {
    DestroyRegularProfileNow(profile);
    return;
  }

  // Off-the-record profiles have DestroyProfileWhenAppropriate() called before
  // their RenderProcessHosts are destroyed, to ensure private data is erased
  // promptly. In this case, defer deletion until all the hosts are gone.
  HostSet profile_hosts = GetHostsForProfile(profile);
  if (profile_hosts.empty()) {
    DestroyOffTheRecordProfileNow(profile);
    return;
  }

  // The instance will destroy itself once all (non-spare) render process
  // hosts referring to it are properly terminated.
  new ProfileDestroyer(profile, &profile_hosts);
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
  if (ResetPendingDestroyers(profile)) {
    // We want to signal this in debug builds so that we don't lose sight of
    // these potential leaks, but we handle it in release so that we don't
    // crash or corrupt profile data on disk.
    NOTREACHED() << "A render process host wasn't destroyed early enough.";
  }
  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile(profile);
  UMA_HISTOGRAM_ENUMERATION("Profile.Destroyer.OffTheRecord",
                            ProfileDestructionType::kImmediately);
}

// static
void ProfileDestroyer::DestroyRegularProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsRegularProfile());
  TRACE_EVENT("shutdown", "ProfileDestroyer::DestroyRegularProfileNow",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
              });

  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose) &&
      content::RenderProcessHost::run_renderer_in_process()) {
    // With DestroyProfileOnBrowserClose and --single-process, we need to clean
    // up the RPH first. Single-process mode does not support multiple Profiles,
    // so this will not interfere with other Profiles.
    content::RenderProcessHost::ShutDownInProcessRenderer();
  }

#if DCHECK_IS_ON()
  // Save the raw pointers of profile and off-the-record profile for DCHECKing
  // on later.
  HostSet profile_hosts = GetHostsForProfile(profile);
  void* profile_ptr = profile;
  std::vector<Profile*> otr_profiles = profile->GetAllOffTheRecordProfiles();
#endif  // DCHECK_IS_ON()

  delete profile;

#if DCHECK_IS_ON()
  // Count the number of hosts that have dangling pointers to the freed Profile
  // and off-the-record Profile.
  const size_t profile_hosts_count = GetHostsForProfile(profile_ptr).size();
  base::debug::Alias(&profile_hosts_count);
  size_t off_the_record_profile_hosts_count = 0;
  for (Profile* otr : otr_profiles)
    off_the_record_profile_hosts_count += GetHostsForProfile(otr).size();
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

bool ProfileDestroyer::ResetPendingDestroyers(Profile* const profile) {
  DCHECK(profile);
  bool found = false;
  if (pending_destroyers_) {
    for (auto* i : *pending_destroyers_) {
      if (i->profile_ == profile) {
        i->profile_ = nullptr;
        found = true;
      }
    }
  }
  return found;
}

ProfileDestroyer::ProfileDestroyer(Profile* const profile, HostSet* hosts)
    : num_hosts_(0), profile_(profile) {
  TRACE_EVENT("shutdown", "ProfileDestroyer::ProfileDestroyer",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile));
                proto->set_host_count_at_creation(hosts->size());
              });
  if (pending_destroyers_ == NULL)
    pending_destroyers_ = new DestroyerSet;
  pending_destroyers_->insert(this);
  for (auto* host : *hosts)
    observations_.AddObservation(host);
  // If we are going to wait for render process hosts, we don't want to do it
  // for longer than kTimerDelaySeconds.
  if (observations_.IsObservingAnySource()) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimerDelaySeconds),
                 base::BindOnce(&ProfileDestroyer::DestroyProfile,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  TRACE_EVENT("shutdown", "ProfileDestroyer::~ProfileDestroyer",
              [&](perfetto::EventContext ctx) {
                auto* proto =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_chrome_profile_destroyer();
                proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile_));
                proto->set_host_count_at_destruction(num_hosts_);
              });

  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  if (profile_)
    DestroyProfileWhenAppropriate(profile_);

  // Don't wait for pending registrations, if any, these hosts are buggy.
  // Note: this can happen, but if so, it's better to crash here than wait
  // for the host to dereference a deleted Profile. http://crbug.com/248625
  UMA_HISTOGRAM_ENUMERATION("Profile.Destroyer.OffTheRecord",
                            num_hosts_
                                ? ProfileDestructionType::kDelayedAndCrashed
                                : ProfileDestructionType::kDelayed);
  CHECK(!observations_.IsObservingAnySource())
      << "Some render process hosts were not destroyed early enough!";
  DCHECK(pending_destroyers_);
  auto iter = pending_destroyers_->find(this);
  DCHECK(iter != pending_destroyers_->end());
  pending_destroyers_->erase(iter);
  if (pending_destroyers_->empty()) {
    delete pending_destroyers_;
    pending_destroyers_ = NULL;
  }
}

void ProfileDestroyer::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  TRACE_EVENT(
      "shutdown", "ProfileDestroyer::RenderProcessHostDestroyed",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_profile_destroyer();
        proto->set_profile_ptr(reinterpret_cast<uint64_t>(profile_));
        proto->set_render_process_host_ptr(reinterpret_cast<uint64_t>(host));
      });
  observations_.RemoveObservation(host);
  if (!observations_.IsObservingAnySource()) {
    // Delay the destruction one step further in case other observers need to
    // look at the profile attached to the host.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ProfileDestroyer::DestroyProfile,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProfileDestroyer::DestroyProfile() {
  // We might have been cancelled externally before the timer expired.
  if (!profile_) {
    delete this;
    return;
  }

  DCHECK(profile_->IsOffTheRecord());
  DCHECK(profile_->GetOriginalProfile());
  profile_->GetOriginalProfile()->DestroyOffTheRecordProfile(profile_);

#if defined(OS_ANDROID)
  // It is possible on Android platform that more than one destroyer
  // is instantiated to delete a single profile. Reset the others to
  // avoid UAF. See https://crbug.com/1029677.
  ResetPendingDestroyers(profile_);
#else
  profile_ = nullptr;
#endif

  // And stop the timer so we can be released early too.
  timer_.Stop();

  delete this;
}

// static
ProfileDestroyer::HostSet ProfileDestroyer::GetHostsForProfile(
    void* const profile_ptr) {
  HostSet hosts;
  for (content::RenderProcessHost::iterator iter(
        content::RenderProcessHost::AllHostsIterator());
      !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    DCHECK(render_process_host);

    if (render_process_host->GetBrowserContext() != profile_ptr)
      continue;

    // Ignore the spare RenderProcessHost.
    if (render_process_host->HostHasNotBeenUsed())
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
    hosts.insert(render_process_host);
  }
  return hosts;
}

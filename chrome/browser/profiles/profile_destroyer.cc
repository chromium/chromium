// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
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

}  // namespace

ProfileDestroyer::DestroyerSet* ProfileDestroyer::pending_destroyers_ = nullptr;

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  TRACE_EVENT2("shutdown", "ProfileDestroyer::DestroyProfileWhenAppropriate",
               "profile", profile, "is_off_the_record",
               profile->IsOffTheRecord());

  DCHECK(profile);
  profile->MaybeSendDestroyedNotification();

  // TODO(https://crbug.com/1033903): If regular profile has OTRs and they have
  // hosts, create a |ProfileDestroyer| instead.
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
  TRACE_EVENT1("shutdown", "ProfileDestroyer::DestroyOffTheRecordProfileNow",
               "profile", profile);
  if (ResetPendingDestroyers(profile)) {
    // We want to signal this in debug builds so that we don't lose sight of
    // these potential leaks, but we handle it in release so that we don't
    // crash or corrupt profile data on disk.
    NOTREACHED() << "A render process host wasn't destroyed early enough.";
  }
  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile(profile);
}

// static
void ProfileDestroyer::DestroyRegularProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsRegularProfile());
  TRACE_EVENT1("shutdown", "ProfileDestroyer::DestroyRegularProfileNow",
               "profile", profile);

#if DCHECK_IS_ON()
  // Save the raw pointers of profile and off-the-record profile for DCHECKing
  // on later.
  HostSet profile_hosts = GetHostsForProfile(profile);
  void* profile_ptr = profile;
  // TODO(https://crbug.com/1033903): Updated to cover all OTR profiles.
  void* otr_profile_ptr = profile->HasPrimaryOTRProfile()
                              ? profile->GetPrimaryOTRProfile()
                              : nullptr;
#endif  // DCHECK_IS_ON()

  delete profile;

#if DCHECK_IS_ON()
  // Count the number of hosts that have dangling pointers to the freed Profile
  // and off-the-record Profile.
  const size_t profile_hosts_count = GetHostsForProfile(profile_ptr).size();
  base::debug::Alias(&profile_hosts_count);
  const size_t off_the_record_profile_hosts_count =
      otr_profile_ptr ? GetHostsForProfile(otr_profile_ptr).size() : 0u;
  base::debug::Alias(&off_the_record_profile_hosts_count);

  // |profile| is not off-the-record, so if |profile_hosts| is not empty then
  // something has leaked a RenderProcessHost, and needs fixing.
  //
  // The exception is that RenderProcessHostImpl::Release() avoids destroying
  // RenderProcessHosts in --single-process mode, to avoid race conditions.
  if (!content::RenderProcessHost::run_renderer_in_process()) {
    DCHECK_EQ(profile_hosts_count, 0u);
#if !defined(OS_CHROMEOS)
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
  TRACE_EVENT2("shutdown", "ProfileDestroyer::ProfileDestroyer", "profile",
               profile, "host_count", hosts->size());
  if (pending_destroyers_ == NULL)
    pending_destroyers_ = new DestroyerSet;
  pending_destroyers_->insert(this);
  for (auto i = hosts->begin(); i != hosts->end(); ++i) {
    (*i)->AddObserver(this);
    // For each of the observations, we bump up our reference count.
    // It will go back to 0 and free us when all hosts are terminated.
    ++num_hosts_;
  }
  // If we are going to wait for render process hosts, we don't want to do it
  // for longer than kTimerDelaySeconds.
  if (num_hosts_) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimerDelaySeconds),
                 base::Bind(&ProfileDestroyer::DestroyProfile,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  TRACE_EVENT1("shutdown", "ProfileDestroyer::~ProfileDestroyer", "profile",
               profile_);

  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  if (profile_)
    DestroyProfileWhenAppropriate(profile_);

  // Don't wait for pending registrations, if any, these hosts are buggy.
  // Note: this can happen, but if so, it's better to crash here than wait
  // for the host to dereference a deleted Profile. http://crbug.com/248625
  CHECK_EQ(0U, num_hosts_) << "Some render process hosts were not "
                           << "destroyed early enough!";
  DCHECK(pending_destroyers_ != NULL);
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
  TRACE_EVENT2("shutdown", "ProfileDestroyer::RenderProcessHostDestroyed",
               "profile", profile_, "render_process_host", host);
  DCHECK_GT(num_hosts_, 0u);
  --num_hosts_;
  if (num_hosts_ == 0) {
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

    TRACE_EVENT2("shutdown", "ProfileDestroyer::GetHostsForProfile", "profile",
                 profile_ptr, "render_process_host", render_process_host);
    hosts.insert(render_process_host);
  }
  return hosts;
}

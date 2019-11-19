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

ProfileDestroyer::DestroyerSet* ProfileDestroyer::pending_destroyers_ = NULL;

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  TRACE_EVENT0("shutdown", "ProfileDestroyer::DestroyProfileWhenAppropriate");

  DCHECK(profile);
  profile->MaybeSendDestroyedNotification();

  // Testing profiles can simply be deleted directly. Some tests don't setup
  // RenderProcessHost correctly and don't necessary run on the UI thread
  // anyway, so we can't iterate them via AllHostsIterator anyway.
  if (profile->AsTestingProfile()) {
    if (profile->IsOffTheRecord())
      profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
    else
      delete profile;
    return;
  }

  HostSet profile_hosts = GetHostsForProfile(profile);
  const bool profile_is_off_the_record = profile->IsOffTheRecord();
  base::debug::Alias(&profile_is_off_the_record);
  const bool profile_has_off_the_record =
      !profile_is_off_the_record && profile->HasOffTheRecordProfile();
  base::debug::Alias(&profile_has_off_the_record);

  // Off-the-record profiles have DestroyProfileWhenAppropriate() called before
  // their RenderProcessHosts are destroyed, to ensure private data is erased
  // promptly. In this case, defer deletion until all the hosts are gone.
  if (profile_is_off_the_record) {
    DCHECK(!profile_has_off_the_record);
    if (profile_hosts.size()) {
      // The instance will destroy itself once all (non-spare) render process
      // hosts referring to it are properly terminated.
      new ProfileDestroyer(profile, &profile_hosts);
    } else {
      if (profile->IsIndependentOffTheRecordProfile())
        delete profile;
      else
        profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
    }
    return;
  }

#if DCHECK_IS_ON()
  // Save the raw pointers of profile and off-the-record profile for DCHECKing
  // on later.
  void* profile_ptr = profile;
  void* otr_profile_ptr =
      profile_has_off_the_record ? profile->GetOffTheRecordProfile() : nullptr;
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

// This can be called to cancel any pending destruction and destroy the profile
// now, e.g., if the parent profile is being destroyed while the incognito one
// still pending...
void ProfileDestroyer::DestroyOffTheRecordProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  if (pending_destroyers_) {
    for (auto i = pending_destroyers_->begin(); i != pending_destroyers_->end();
         ++i) {
      if ((*i)->profile_ == profile) {
        // We want to signal this in debug builds so that we don't lose sight of
        // these potential leaks, but we handle it in release so that we don't
        // crash or corrupt profile data on disk.
        NOTREACHED() << "A render process host wasn't destroyed early enough.";
        (*i)->profile_ = NULL;
        break;
      }
    }
  }
  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
}

ProfileDestroyer::ProfileDestroyer(Profile* const profile, HostSet* hosts)
    : num_hosts_(0), profile_(profile) {
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
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kTimerDelaySeconds),
                 base::Bind(&ProfileDestroyer::DestroyProfile,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  if (profile_)
    DestroyProfileWhenAppropriate(profile_);

#ifdef NDEBUG
  // Don't wait for pending registrations, if any, these hosts are buggy.
  // Note: this can happen, but if so, it's better to crash here than wait
  // for the host to dereference a deleted Profile. http://crbug.com/248625
  CHECK_EQ(0U, num_hosts_) << "Some render process hosts were not "
                           << "destroyed early enough!";
#endif  // NDEBUG
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
  DCHECK(num_hosts_ > 0);
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
  if (profile_->IsIndependentOffTheRecordProfile())
    delete profile_;
  else
    profile_->GetOriginalProfile()->DestroyOffTheRecordProfile();

  profile_ = nullptr;

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

    hosts.insert(render_process_host);
  }
  return hosts;
}

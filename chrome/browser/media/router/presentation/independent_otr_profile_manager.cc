// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/independent_otr_profile_manager.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

IndependentOTRProfileManager::OTRProfileRegistration::
    ~OTRProfileRegistration() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  manager_->UnregisterProfile(profile_);
}

IndependentOTRProfileManager::OTRProfileRegistration::OTRProfileRegistration(
    IndependentOTRProfileManager* manager,
    Profile* profile)
    : manager_(manager), profile_(profile) {
  DCHECK(manager_);
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
}

// static
IndependentOTRProfileManager* IndependentOTRProfileManager::GetInstance() {
  return base::Singleton<IndependentOTRProfileManager>::get();
}

std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>
IndependentOTRProfileManager::CreateFromOriginalProfile(
    Profile* original_profile,
    ProfileDestroyedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(original_profile);
  DCHECK(!original_profile->IsOffTheRecord());
  DCHECK(!callback.is_null());
  if (!HasDependentProfiles(original_profile))
    observed_original_profiles_.Add(original_profile);
  auto* otr_profile = original_profile->CreateOffTheRecordProfile();
  auto entry = refcounts_map_.emplace(otr_profile, 1);
  auto callback_entry =
      callbacks_map_.emplace(otr_profile, std::move(callback));
  DCHECK(entry.second);
  DCHECK(callback_entry.second);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(otr_profile),
      content::NotificationService::NoDetails());

  return base::WrapUnique(new OTRProfileRegistration(this, otr_profile));
}

void IndependentOTRProfileManager::OnBrowserAdded(Browser* browser) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* profile = browser->profile();
  auto entry = refcounts_map_.find(profile);
  if (entry != refcounts_map_.end()) {
    ++entry->second;
  }
}

void IndependentOTRProfileManager::OnBrowserRemoved(Browser* browser) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* profile = browser->profile();
  auto entry = refcounts_map_.find(profile);
  if (entry == refcounts_map_.end()) {
    return;
  }
  --entry->second;
  if (entry->second == 0) {
    // The is the last Browser that references |profile| _and_ the original
    // registration that owned |profile| has already been destroyed.  Since the
    // owner of the registration is also responsible for the callback, there
    // should be no callback at this point.
    DCHECK(callbacks_map_.find(profile) == callbacks_map_.end());
    // This will be called from within ~Browser so we can't delete its profile
    // immediately in case it has a (possibly indirect) dependency on the
    // profile.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, entry->first);
    auto* original_profile = entry->first->GetOriginalProfile();
    refcounts_map_.erase(entry);
    if (!HasDependentProfiles(original_profile))
      observed_original_profiles_.Remove(original_profile);
  }
}

IndependentOTRProfileManager::IndependentOTRProfileManager() {
  BrowserList::AddObserver(this);
}

IndependentOTRProfileManager::~IndependentOTRProfileManager() {
  BrowserList::RemoveObserver(this);
  // This should be destroyed after all Browser objects, so any remaining
  // refcounts should be due to registrations, which each have a corresponding
  // callback.
  DCHECK(std::all_of(refcounts_map_.begin(), refcounts_map_.end(),
                     [](const std::pair<Profile*, int32_t>& entry) {
                       return entry.second == 1;
                     }));
  DCHECK(refcounts_map_.size() == callbacks_map_.size());
}

bool IndependentOTRProfileManager::HasDependentProfiles(
    Profile* profile) const {
  return std::find_if(refcounts_map_.begin(), refcounts_map_.end(),
                      [profile](const std::pair<Profile*, int32_t>& entry) {
                        return entry.first->GetOriginalProfile() == profile;
                      }) != refcounts_map_.end();
}

void IndependentOTRProfileManager::UnregisterProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto entry = refcounts_map_.find(profile);
  DCHECK(entry != refcounts_map_.end());
  callbacks_map_.erase(profile);
  --entry->second;
  if (entry->second == 0) {
    auto* original_profile = profile->GetOriginalProfile();
    ProfileDestroyer::DestroyProfileWhenAppropriate(entry->first);
    refcounts_map_.erase(entry);
    if (!HasDependentProfiles(original_profile))
      observed_original_profiles_.Remove(original_profile);
  }
}

void IndependentOTRProfileManager::OnProfileWillBeDestroyed(Profile* profile) {
  for (auto it = callbacks_map_.begin(); it != callbacks_map_.end();) {
    if (profile != it->first->GetOriginalProfile()) {
      ++it;
      continue;
    }
    // If the registration is destroyed from within this callback, we don't want
    // to double-erase.  If it isn't, we still need to erase the callback entry.
    auto* otr_profile = it->first;
    auto callback = std::move(it->second);
    it = callbacks_map_.erase(it);
    std::move(callback).Run(otr_profile);
  }
}

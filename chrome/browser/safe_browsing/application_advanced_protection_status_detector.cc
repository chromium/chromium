// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/application_advanced_protection_status_detector.h"

#include <utility>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"

namespace safe_browsing {
namespace {

bool IsIgnoredProfile(Profile* profile) {
  // We only care about regular profiles, not incognito, guest, or system
  // profiles.
  return profile->IsOffTheRecord() || profile->IsGuestSession() ||
         profile->IsSystemProfile();
}

}  // namespace
class ApplicationAdvancedProtectionStatusDetector::
    ProfileAdvancedProtectionObserver
    : public AdvancedProtectionStatusManager::StatusChangedObserver {
 public:
  ProfileAdvancedProtectionObserver(
      ApplicationAdvancedProtectionStatusDetector* detector,
      Profile* profile)
      : detector_(detector), profile_(profile) {
    AdvancedProtectionStatusManager* ap_manager =
        AdvancedProtectionStatusManagerFactory::GetForProfile(profile_);
    if (ap_manager) {
      observation_.Observe(ap_manager);
      latest_status_ = ap_manager->IsUnderAdvancedProtection();
    }
  }

  ~ProfileAdvancedProtectionObserver() override = default;

  // AdvancedProtectionStatusManager::StatusChangedObserver:
  void OnAdvancedProtectionStatusChanged(bool enabled) override {
    if (latest_status_ != enabled) {
      latest_status_ = enabled;
      detector_->OnAdvancedProtectionStatusChangedForSingleProfile(enabled);
    }
  }

  bool latest_status() const { return latest_status_; }

  void Reset() { observation_.Reset(); }

 private:
  bool latest_status_ = false;
  raw_ptr<ApplicationAdvancedProtectionStatusDetector> detector_;
  raw_ptr<Profile> profile_;
  base::ScopedObservation<
      AdvancedProtectionStatusManager,
      AdvancedProtectionStatusManager::StatusChangedObserver>
      observation_{this};
};

ApplicationAdvancedProtectionStatusDetector::
    ApplicationAdvancedProtectionStatusDetector(ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {
  if (!profile_manager_) {
    return;
  }

  profile_manager_observation_.Observe(profile_manager_);
  for (Profile* profile : profile_manager_->GetLoadedProfiles()) {
    AddProfile(profile);
  }
}

ApplicationAdvancedProtectionStatusDetector::
    ~ApplicationAdvancedProtectionStatusDetector() = default;

bool ApplicationAdvancedProtectionStatusDetector::IsUnderAdvancedProtection()
    const {
  return advanced_protection_profile_count_ > 0;
}

void ApplicationAdvancedProtectionStatusDetector::AddObserver(
    StatusObserver* observer) {
  observers_.AddObserver(observer);
}

void ApplicationAdvancedProtectionStatusDetector::RemoveObserver(
    StatusObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ApplicationAdvancedProtectionStatusDetector::OnProfileAdded(
    Profile* profile) {
  AddProfile(profile);
}

void ApplicationAdvancedProtectionStatusDetector::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  profile_observations_.RemoveAllObservations();
  // application_advanced_protection_status_ = false;
  advanced_protection_profile_count_ = 0;
  NotifyObservers();
}

void ApplicationAdvancedProtectionStatusDetector::OnProfileWillBeDestroyed(
    Profile* profile) {
  if (IsIgnoredProfile(profile)) {
    return;
  }
  // All KeyedServices are still valid during `OnProfileWillBeDestroyed`.
  auto profile_ap_observer_it = profile_ap_observers_.find(profile);
  DCHECK(profile_ap_observer_it != profile_ap_observers_.end());
  // Take ownership of the observation.
  std::unique_ptr<ProfileAdvancedProtectionObserver> profile_ap_observer =
      std::move(profile_ap_observer_it->second);
  profile_ap_observers_.erase(profile_ap_observer_it);

  // If this profile is under Advanced Protection, check if the remaining
  // profiles have Advanced Protection and update status accordingly.
  if (profile_ap_observer->latest_status()) {
    OnAdvancedProtectionStatusChangedForSingleProfile(false);
  }
  profile_ap_observer->Reset();
  profile_observations_.RemoveObservation(profile);
}

void ApplicationAdvancedProtectionStatusDetector::
    OnAdvancedProtectionStatusChangedForSingleProfile(bool status) {
  if (status) {
    advanced_protection_profile_count_++;
    if (advanced_protection_profile_count_ == 1) {
      NotifyObservers();
    }
  } else {
    advanced_protection_profile_count_--;
    DCHECK_GE(advanced_protection_profile_count_, 0);
    if (advanced_protection_profile_count_ == 0) {
      NotifyObservers();
    }
  }
}

void ApplicationAdvancedProtectionStatusDetector::AddProfile(Profile* profile) {
  if (IsIgnoredProfile(profile)) {
    return;
  }
  // Start observing the profile.
  profile_observations_.AddObservation(profile);
  // Start observing `AdvancedProtectionStatusManager` for the `profile`.
  auto profile_ap_observer =
      std::make_unique<ProfileAdvancedProtectionObserver>(this, profile);
  if (profile_ap_observer->latest_status()) {
    advanced_protection_profile_count_++;
    if (advanced_protection_profile_count_ == 1) {
      NotifyObservers();
    }
  }
  DCHECK(profile_ap_observers_.find(profile) == profile_ap_observers_.end());
  profile_ap_observers_[profile] = std::move(profile_ap_observer);
}

void ApplicationAdvancedProtectionStatusDetector::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnApplicationAdvancedProtectionStatusChanged(
        IsUnderAdvancedProtection());
  }
}

}  // namespace safe_browsing

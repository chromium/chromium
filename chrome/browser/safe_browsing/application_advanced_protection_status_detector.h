// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_APPLICATION_ADVANCED_PROTECTION_STATUS_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_APPLICATION_ADVANCED_PROTECTION_STATUS_DETECTOR_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

class Profile;
class ProfileManager;
namespace safe_browsing {

// LINT.IfChange(ApplicationAdvancedProtectionEvent)
enum class ApplicationAdvancedProtectionEvent {
  kInitialized = 0,
  kProfileAdded = 1,
  kProfileRemoved = 2,
  kProfileAdvancedProtectionStatusChanged = 3,
  kMaxValue = kProfileAdvancedProtectionStatusChanged,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:ApplicationAdvancedProtectionEvent)

// This class is responsible for monitoring the advanced protection status
// of all profiles managed by ProfileManager. It aggregates the status and
// notifies observers if any profile is under advanced protection.
class ApplicationAdvancedProtectionStatusDetector
    : public ProfileManagerObserver,
      public ProfileObserver {
 public:
  // Observer to track changes in the enabled/disabled status of application
  // Advanced
  // Protection.
  class StatusObserver : public base::CheckedObserver {
   public:
    virtual void OnApplicationAdvancedProtectionStatusChanged(bool enabled) = 0;
  };
  explicit ApplicationAdvancedProtectionStatusDetector(
      ProfileManager* profile_manager);

  ApplicationAdvancedProtectionStatusDetector(
      const ApplicationAdvancedProtectionStatusDetector&) = delete;
  ApplicationAdvancedProtectionStatusDetector& operator=(
      const ApplicationAdvancedProtectionStatusDetector&) = delete;

  ~ApplicationAdvancedProtectionStatusDetector() override;

  // Returns true if at least one of the profiles in ProfileManager is under
  // Advanced Protection.
  bool IsUnderAdvancedProtection() const;

  void AddObserver(StatusObserver* observer);
  void RemoveObserver(StatusObserver* observer);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void SetIsUnderAdvancedProtectionForTesting(bool enabled);

 private:
  class ProfileAdvancedProtectionObserver;

  void AddProfile(Profile* profile, ApplicationAdvancedProtectionEvent event);
  void OnAdvancedProtectionStatusChangedForSingleProfile(bool status);
  void HandleStatusChangedForSingleProfile(
      bool status,
      ApplicationAdvancedProtectionEvent event);
  void NotifyObservers();

  // Observes the ProfileManager for profile additions and removals.
  raw_ptr<ProfileManager> profile_manager_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  // Observes all Profiles for their destruction.
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
  // Maps each Profile to its corresponding ProfileAdvancedProtectionObserver.
  std::map<Profile*, std::unique_ptr<ProfileAdvancedProtectionObserver>>
      profile_ap_observers_;

  // The number of profiles under advanced protection.
  int advanced_protection_profile_count_ = 0;

  // List of observers to be notified when the overall advanced protection
  // status changes.
  base::ObserverList<StatusObserver, /*check_empty=*/true> observers_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_APPLICATION_ADVANCED_PROTECTION_STATUS_DETECTOR_H_

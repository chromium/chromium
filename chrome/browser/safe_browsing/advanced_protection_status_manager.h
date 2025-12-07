// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace safe_browsing {

// Responsible for keeping track of advanced protection status of the profile.
// For incognito profile Chrome returns users' advanced protection status
// of its original profile.
class AdvancedProtectionStatusManager : public KeyedService {
 public:
  // Observer to track changes in the enabled/disabled status of Advanced
  // Protection. Observers must use IsUnderAdvancedProtection() to check the
  // status.
  class StatusChangedObserver : public base::CheckedObserver {
   public:
    virtual void OnAdvancedProtectionStatusChanged(bool enabled) = 0;
  };

  AdvancedProtectionStatusManager();

  virtual void SetAdvancedProtectionStatusForTesting(bool enrolled) = 0;

  // Returns whether the unconsented primary account of the associated profile
  // is under Advanced Protection.
  virtual bool IsUnderAdvancedProtection() const = 0;

  // Adds and removes observers to observe enabled/disabled status changes.
  void AddObserver(StatusChangedObserver* observer);
  void RemoveObserver(StatusChangedObserver* observer);

 protected:
  ~AdvancedProtectionStatusManager() override;

  void NotifyObserversStatusChanged();

 private:
  base::ObserverList<StatusChangedObserver> observers_;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

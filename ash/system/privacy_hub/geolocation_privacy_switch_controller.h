// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"

class PrefChangeRegistrar;

namespace ash {

class ASH_EXPORT GeolocationPrivacySwitchController : public SessionObserver {
 public:
  GeolocationPrivacySwitchController();

  GeolocationPrivacySwitchController(
      const GeolocationPrivacySwitchController&) = delete;
  GeolocationPrivacySwitchController& operator=(
      const GeolocationPrivacySwitchController&) = delete;

  ~GeolocationPrivacySwitchController() override;

  // Gets the singleton instance that lives within `Shell` if available.
  static GeolocationPrivacySwitchController* Get();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Apps that want to actively use geolocation should register and deregister
  // using the following methods. They are used to decide whether a notification
  // that an app wants to use geolocation should be used. System usages like
  // time-zones should not use this mechanism as they are permanently active.
  void OnAppStartsUsingGeolocation(const std::u16string& app_name);
  void OnAppStopsUsingGeolocation(const std::u16string& app_name);

  // Returns the names of the apps that want to actively use geolocation (if
  // there is more than `max_count` of such apps, first max_count names are
  // returned ).
  std::vector<std::u16string> GetActiveApps(size_t max_count) const;

 private:
  // Called when the preference value is changed.
  void OnPreferenceChanged();
  // Called when the notification should be updated (either preference changed
  // or apps started/stopped attempting to use geolocation).
  void UpdateNotification();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  int usage_cnt_{};
  std::map<std::u16string, int> usage_per_app_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_

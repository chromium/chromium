// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

class PrefRegistrySimple;

namespace ash {

// Controller class to manage gesture education notification. This notification
// shows up once to provide the user with information about new gestures added
// to chrome os for easier navigation.
class ASH_EXPORT GestureEducationNotificationController
    : public SessionObserver,
      public TabletModeObserver {
 public:
  GestureEducationNotificationController();
  ~GestureEducationNotificationController() override;

  // Shows the gesture education notification if it needs to be shown.
  void MaybeShowNotification();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletControllerDestroyed() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

 private:
  friend class GestureEducationNotificationControllerTest;

  void GenerateGestureEducationNotification();
  std::u16string GetNotificationTitle() const;
  std::u16string GetNotificationMessage() const;
  void HandleNotificationClick();

  void ResetPrefForTest();

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};

  PrefService* active_user_prefs_ = nullptr;  // Not owned.

  static const char kNotificationId[];

  base::WeakPtrFactory<GestureEducationNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_

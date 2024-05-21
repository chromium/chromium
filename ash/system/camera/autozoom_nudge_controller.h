// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/camera/autozoom_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class AutozoomControllerImpl;

class ASH_EXPORT AutozoomNudgeController : public AutozoomObserver,
                                           public SessionObserver {
 public:
  explicit AutozoomNudgeController(AutozoomControllerImpl* controller);
  AutozoomNudgeController(const AutozoomNudgeController&) = delete;
  AutozoomNudgeController& operator=(const AutozoomNudgeController&) = delete;
  ~AutozoomNudgeController() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Gets whether the user had enabled autozoom before.
  bool GetHadEnabled(PrefService* prefs);
  // Gets the number of times the nudge has been shown.
  int GetShownCount(PrefService* prefs);
  // Gets the last time the nudge was shown.
  base::Time GetLastShownTime(PrefService* prefs);
  // Checks whether another nudge can be shown.
  bool ShouldShowNudge(PrefService* prefs);
  // Gets the current time.
  base::Time GetTime();
  // Resets nudge state and show nudge timer.
  void HandleNudgeShown();

  // AutozoomObserver:
  void OnAutozoomStateChanged(
      cros::mojom::CameraAutoFramingState state) override;
  void OnAutozoomControlEnabledChanged(bool enabled) override;

  // Owned by ash/Shell.
  const raw_ptr<AutozoomControllerImpl> autozoom_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_CONTROLLER_H_

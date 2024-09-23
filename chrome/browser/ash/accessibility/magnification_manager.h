// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "ui/events/event_handler.h"
#include "ui/views/accessibility/ax_event_observer.h"

class PrefChangeRegistrar;

namespace gfx {
class Point;
class Rect;
}

namespace ash {

// MagnificationManager controls the Fullscreen and Docked magnifier from
// chrome-browser side (not ash side).
//
// MagnificationManager does below for Fullscreen magnifier:
// TODO(warx): Move to ash.
//   - Watch logged-in. Changes the behavior between the login screen and user
//     desktop.
//   - Watch change of the pref. When the pref changes, the setting of the
//     magnifier will interlock with it.
class MagnificationManager
    : public session_manager::SessionManagerObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public ProfileObserver,
      public ui::EventHandler,
      public views::AXEventObserver {
 public:
  MagnificationManager(const MagnificationManager&) = delete;
  MagnificationManager& operator=(const MagnificationManager&) = delete;

  // Creates an instance of MagnificationManager. This should be called once.
  static void Initialize();

  // Deletes the existing instance of MagnificationManager.
  static void Shutdown();

  // Returns the existing instance. If there is no instance, returns NULL.
  static MagnificationManager* Get();

  // Returns if the Fullscreen magnifier is enabled.
  bool IsMagnifierEnabled() const;

  // Enables the Fullscreen magnifier.
  void SetMagnifierEnabled(bool enabled);

  // Returns if the Docked magnifier is enabled.
  bool IsDockedMagnifierEnabled() const;

  // Enables the Docked magnifier.
  void SetDockedMagnifierEnabled(bool enabled);

  // Saves the Fullscreen magnifier scale to the pref.
  void SaveScreenMagnifierScale(double scale);

  // Loads the Fullscreen magnifier scale from the pref.
  double GetSavedScreenMagnifierScale() const;

  // Move magnifier to ensure rect is within viewport if a magnifier is enabled.
  void HandleMoveMagnifierToRectIfEnabled(const gfx::Rect& rect);

  // Move magnified region to center on point if a magnifier is enabled.
  void HandleMagnifierCenterOnPointIfEnabled(const gfx::Point& point_in_screen);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;

  void SetProfileForTest(Profile* profile);

 private:
  MagnificationManager();
  ~MagnificationManager() override;

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  void SetMagnifierEnabledInternal(bool enabled);
  void SetMagnifierScaleInternal(double scale);
  void SetMagnifierMouseFollowingModeInternal(
      MagnifierMouseFollowingMode mouse_following_mode);
  void UpdateMagnifierFromPrefs();
  void UpdateDockedMagnifierFromPrefs();

  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // Last mouse event time - used for ignoring focus changes for a few
  // milliseconds after the last mouse event.
  base::TimeTicks last_mouse_event_;

  bool fullscreen_magnifier_enabled_ = false;
  double scale_ = 0.0;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::WeakPtrFactory<MagnificationManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

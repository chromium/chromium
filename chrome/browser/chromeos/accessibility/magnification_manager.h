// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/accessibility/ax_event_observer.h"

class PrefChangeRegistrar;

namespace chromeos {

// MagnificationManager controls the Fullscreen and Docked magnifier from
// chrome-browser side (not ash side).
//
// MagnificationManager does below for Fullscreen magnifier:
// TODO(warx): Move to ash.
//   - Watch logged-in. Changes the behavior between the login screen and user
//     desktop.
//   - Watch change of the pref. When the pref changes, the setting of the
//     magnifier will interlock with it.
//
// MagnificationManager also observes focus changed in page and calls Ash when
// either Fullscreen or Docked magnifier is enabled.
class MagnificationManager
    : public content::NotificationObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public ProfileObserver,
      public views::AXEventObserver {
 public:
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

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;

  void SetProfileForTest(Profile* profile);

 private:
  MagnificationManager();
  ~MagnificationManager() override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  void SetMagnifierEnabledInternal(bool enabled);
  void SetMagnifierKeepFocusCenteredInternal(bool keep_focus_centered);
  void SetMagnifierScaleInternal(double scale);
  void UpdateMagnifierFromPrefs();
  void UpdateDockedMagnifierFromPrefs();

  // Called when received content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE.
  void HandleFocusChangedInPage(const content::NotificationDetails& details);

  // Called in response to AXEventObserver.
  void HandleFocusChanged(const gfx::Rect& bounds_in_screen, bool is_editable);

  Profile* profile_ = nullptr;
  ScopedObserver<Profile, ProfileObserver> profile_observer_{this};

  bool fullscreen_magnifier_enabled_ = false;
  bool keep_focus_centered_ = false;
  double scale_ = 0.0;

  content::NotificationRegistrar registrar_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::WeakPtrFactory<MagnificationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MagnificationManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

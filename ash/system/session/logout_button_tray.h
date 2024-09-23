// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_
#define ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace views {
class MdTextButton;
}

namespace ash {
class Shelf;

// Adds a logout button to the shelf's status area if enabled by the
// kShowLogoutButtonInTray pref.
class ASH_EXPORT LogoutButtonTray : public TrayBackgroundView,
                                    public SessionObserver {
  METADATA_HEADER(LogoutButtonTray, TrayBackgroundView)

 public:
  explicit LogoutButtonTray(Shelf* shelf);

  LogoutButtonTray(const LogoutButtonTray&) = delete;
  LogoutButtonTray& operator=(const LogoutButtonTray&) = delete;

  ~LogoutButtonTray() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // TrayBackgroundView:
  void UpdateAfterLoginStatusChange() override;
  void UpdateLayout() override;
  void UpdateBackground() override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  // No need to override since this view doesn't have an active/inactive state.
  // Clicking on it will log out of the session and make this view disappear.
  void UpdateTrayItemColor(bool is_active) override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  views::MdTextButton* button_for_test() const { return button_; }

 private:
  void UpdateShowLogoutButtonInTray();
  void UpdateLogoutDialogDuration();
  void UpdateVisibility();
  void UpdateButtonTextAndImage();

  void ButtonPressed();

  raw_ptr<views::MdTextButton> button_;
  bool show_logout_button_in_tray_ = false;
  base::TimeDelta dialog_duration_;

  // Observes user profile prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_

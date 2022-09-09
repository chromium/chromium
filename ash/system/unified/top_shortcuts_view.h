// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "quick_settings_button_base.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class PrefRegistrySimple;

namespace ash {

class CollapseButton;
class TopShortcutsViewTest;
class UnifiedSystemTrayController;

// Container for the top shortcut buttons. The view may narrow gaps between
// buttons when there's not enough space. When those doesn't fit in the view
// even after that, the sign-out button will be resized.
class TopShortcutButtonContainer : public views::View,
                                   public views::ViewObserver {
 public:
  TopShortcutButtonContainer();

  TopShortcutButtonContainer(const TopShortcutButtonContainer&) = delete;
  TopShortcutButtonContainer& operator=(const TopShortcutButtonContainer&) =
      delete;

  ~TopShortcutButtonContainer() override;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  views::View* AddUserAvatarButton(
      std::unique_ptr<views::View> user_avatar_button);
  // Add the sign-out button, which can be resized upon layout.
  views::View* AddSignOutButton(std::unique_ptr<views::View> sign_out_button);

  // views::ViewObserver:
  void OnChildViewAdded(View* observed_view, View* child) override;

 private:
  views::View* user_avatar_button_ = nullptr;
  views::View* sign_out_button_ = nullptr;
};

// Top shortcuts view shown on the top of UnifiedSystemTrayView.
class ASH_EXPORT TopShortcutsView : public views::View {
 public:
  explicit TopShortcutsView(UnifiedSystemTrayController* controller);

  TopShortcutsView(const TopShortcutsView&) = delete;
  TopShortcutsView& operator=(const TopShortcutsView&) = delete;
  ~TopShortcutsView() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Change the expanded state. CollapseButton icon will rotate.
  void SetExpandedAmount(double expanded_amount);

  // views::View
  const char* GetClassName() const override;

 private:
  friend class TopShortcutsViewTest;

  // Disables/Enables the |settings_button_| based on kSettingsIconEnabled pref.
  void UpdateSettingsButtonState();

  // Owned by views hierarchy.
  TopShortcutButtonContainer* container_ = nullptr;
  views::Button* settings_button_ = nullptr;
  CollapseButton* collapse_button_ = nullptr;

  std::unique_ptr<QuickSettingsButtonDelegate> user_avatar_button_delegate_;
  std::unique_ptr<QuickSettingsButtonDelegate> sign_out_button_delegate_;
  std::unique_ptr<QuickSettingsButtonDelegate> lock_button_delegate_;
  std::unique_ptr<QuickSettingsButtonDelegate> settings_button_delegate_;
  std::unique_ptr<QuickSettingsButtonDelegate> power_button_delegate_;

  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

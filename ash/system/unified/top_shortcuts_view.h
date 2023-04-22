// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class PrefRegistrySimple;

namespace views {
class Button;
}

namespace ash {

class CollapseButton;
class IconButton;
class TopShortcutsViewTest;
class UnifiedSystemTrayController;

// Container for the top shortcut buttons. The view may narrow gaps between
// buttons when there's not enough space. When those doesn't fit in the view
// even after that, the sign-out button will be resized.
class TopShortcutButtonContainer : public views::View {
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
  views::Button* AddSignOutButton(
      std::unique_ptr<views::Button> sign_out_button);

 private:
  raw_ptr<views::View, ExperimentalAsh> user_avatar_button_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> sign_out_button_ = nullptr;
};

// Top shortcuts view shown on the top of UnifiedSystemTrayView.
class ASH_EXPORT TopShortcutsView : public views::View,
                                    public views::ViewObserver {
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

  // views::ViewObserver:
  void OnChildViewAdded(View* observed_view, View* child) override;

  // Disables/Enables the |settings_button_| based on kSettingsIconEnabled pref.
  void UpdateSettingsButtonState();

  // Owned by views hierarchy.
  raw_ptr<views::View, ExperimentalAsh> user_avatar_button_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> sign_out_button_ = nullptr;
  raw_ptr<TopShortcutButtonContainer, ExperimentalAsh> container_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> lock_button_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> settings_button_ = nullptr;
  raw_ptr<IconButton, ExperimentalAsh> power_button_ = nullptr;
  raw_ptr<CollapseButton, ExperimentalAsh> collapse_button_ = nullptr;

  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

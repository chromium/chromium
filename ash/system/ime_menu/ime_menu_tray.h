// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_MENU_IME_MENU_TRAY_H_
#define ASH_SYSTEM_IME_MENU_IME_MENU_TRAY_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/public/mojom/ime_keyset.mojom.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {
class ImeController;
class ImeListView;

// A button in the tray which displays the short name of the currently-activated
// IME (e.g., 'GB', 'US'). Clicking this button opens the opt-in IME menu,
// a standalone bubble displaying a list of available IMEs along with buttons
// for emoji, handwriting, and voice.
class ASH_EXPORT ImeMenuTray : public TrayBackgroundView,
                               public IMEObserver,
                               public KeyboardControllerObserver,
                               public VirtualKeyboardObserver {
 public:
  explicit ImeMenuTray(Shelf* shelf);
  ~ImeMenuTray() override;

  // Shows the virtual keyboard with the given keyset: emoji, handwriting or
  // voice.
  void ShowKeyboardWithKeyset(chromeos::input_method::mojom::ImeKeyset keyset);

  // Returns true if the menu should show emoji, handwriting and voice buttons
  // on the bottom. Otherwise, the menu will show a 'Customize...' bottom row
  // for non-MD UI, and a Settings button in the title row for MD.
  bool ShouldShowBottomButtons();

  // Returns whether the virtual keyboard toggle should be shown in shown in the
  // opt-in IME menu.
  bool ShouldShowKeyboardToggle() const;

  // TrayBackgroundView:
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;
  const char* GetClassName() const override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_activated) override;

  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // KeyboardControllerObserver:
  void OnKeyboardHidden(bool is_temporary_hide) override;

  // VirtualKeyboardObserver:
  void OnKeyboardSuppressionChanged(bool suppressed) override;

 private:
  friend class ImeMenuTrayTest;

  // Show the IME menu bubble immediately. Set |show_by_click| to true if bubble
  // is shown by mouse or gesture click.
  void ShowImeMenuBubbleInternal(bool show_by_click);

  // Updates the text of the label on the tray.
  void UpdateTrayLabel();
  void CreateLabel();
  void CreateImageView();

  ImeController* ime_controller_;

  // Bubble for default and detailed views.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  ImeListView* ime_list_view_;

  // Only one of |label_| and |image_view_| can be non null at the same time.
  views::Label* label_;
  views::ImageView* image_view_;

  bool keyboard_suppressed_;
  bool show_bubble_after_keyboard_hidden_;
  bool is_emoji_enabled_;
  bool is_handwriting_enabled_;
  bool is_voice_enabled_;

  base::WeakPtrFactory<ImeMenuTray> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImeMenuTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_MENU_IME_MENU_TRAY_H_

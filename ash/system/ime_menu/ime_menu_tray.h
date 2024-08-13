// Copyright 2016 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/ime_keyset.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {
class ImeControllerImpl;
class ImeListView;

// A button in the tray which displays the short name of the currently-activated
// IME (e.g., 'GB', 'US'). Clicking this button opens the opt-in IME menu,
// a standalone bubble displaying a list of available IMEs along with buttons
// for emoji, handwriting, and voice.
class ASH_EXPORT ImeMenuTray : public TrayBackgroundView,
                               public IMEObserver,
                               public KeyboardControllerObserver,
                               public VirtualKeyboardObserver {
  METADATA_HEADER(ImeMenuTray, TrayBackgroundView)

 public:
  explicit ImeMenuTray(Shelf* shelf);
  ImeMenuTray(const ImeMenuTray&) = delete;
  ImeMenuTray& operator=(const ImeMenuTray&) = delete;
  ~ImeMenuTray() override;

  // Callback called when this TrayBackgroundView is pressed.
  void OnTrayButtonPressed();

  // Shows the virtual keyboard with the given keyset: emoji, handwriting or
  // voice.
  void ShowKeyboardWithKeyset(input_method::ImeKeyset keyset);

  // Returns whether the virtual keyboard toggle should be shown in shown in the
  // opt-in IME menu.
  bool ShouldShowKeyboardToggle() const;

  // TrayBackgroundView:
  void OnThemeChanged() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void AddedToWidget() override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_activated) override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // KeyboardControllerObserver:
  void OnKeyboardHidden(bool is_temporary_hide) override;

  // VirtualKeyboardObserver:
  void OnKeyboardSuppressionChanged(bool suppressed) override;

  // Returns true if any of the bottom buttons in the IME tray bubble are shown.
  // Only used in test code.
  bool AnyBottomButtonShownForTest() const;

 private:
  friend class ImeMenuTrayTest;

  // Show the IME menu bubble immediately.
  void ShowImeMenuBubbleInternal();

  // Updates the text of the label on the tray.
  void UpdateTrayLabel();
  void CreateLabel();
  void CreateImageView();

  // Updates the color of `image_view_` if `is_image` is true or the color of
  // `label_` otherwise.
  void UpdateTrayImageOrLabelColor(bool is_image);

  raw_ptr<ImeControllerImpl> ime_controller_;

  // Bubble for default and detailed views.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  raw_ptr<ImeListView> ime_list_view_;

  // Only one of |label_| and |image_view_| can be non null at the same time.
  raw_ptr<views::Label> label_;
  raw_ptr<views::ImageView> image_view_;

  bool keyboard_suppressed_;
  bool show_bubble_after_keyboard_hidden_;
  bool is_emoji_enabled_;
  bool is_handwriting_enabled_;
  bool is_voice_enabled_;

  base::WeakPtrFactory<ImeMenuTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_MENU_IME_MENU_TRAY_H_

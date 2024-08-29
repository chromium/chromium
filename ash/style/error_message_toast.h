// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ERROR_MESSAGE_TOAST_H_
#define ASH_STYLE_ERROR_MESSAGE_TOAST_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

// Displays error message in a toast view. This is usually placed at the
// bottom of its parent bubble.
class ASH_EXPORT ErrorMessageToast : public views::FlexLayoutView {
  METADATA_HEADER(ErrorMessageToast, views::FlexLayoutView)

 public:
  // Used for `action_button_` that indicates what to expect on click.
  enum class ButtonActionType { kDismiss, kReload };

  ErrorMessageToast(
      views::Button::PressedCallback callback,
      const std::u16string& error_message,
      ButtonActionType type,
      ui::ColorId background_color_id = cros_tokens::kCrosSysSystemBase);
  ErrorMessageToast(const ErrorMessageToast&) = delete;
  ErrorMessageToast& operator=(const ErrorMessageToast&) =
      delete;
  ~ErrorMessageToast() override = default;

  views::Label* error_message_label() { return error_message_label_; }
  views::LabelButton* action_button() { return action_button_; }

  // Updates the error message view to display proportionally to the given
  // `container_bounds` and place it at the bottom of the container with the
  // `padding`. Note that the top of the `padding` is not used.
  void UpdateBoundsToContainer(const gfx::Rect& container_bounds,
                               const gfx::Insets& padding = gfx::Insets());

  std::u16string GetMessageForTest() const;
  views::LabelButton* GetButtonForTest() const { return action_button_; }

 private:
  raw_ptr<views::Label> error_message_label_ = nullptr;
  raw_ptr<views::LabelButton> action_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_STYLE_ERROR_MESSAGE_TOAST_H_

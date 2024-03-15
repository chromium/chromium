// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_ERROR_MESSAGE_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_ERROR_MESSAGE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

// Displays error message at the bottom of the glanceables bubble. Used in
// tasks bubbles.
class ASH_EXPORT GlanceablesErrorMessageView : public views::FlexLayoutView {
  METADATA_HEADER(GlanceablesErrorMessageView, views::FlexLayoutView)

 public:
  // Used for `action_button_` that indicates what to expect on click.
  enum class ButtonActionType { kDismiss, kReload };

  GlanceablesErrorMessageView(views::Button::PressedCallback callback,
                              const std::u16string& error_message,
                              ButtonActionType type);
  GlanceablesErrorMessageView(const GlanceablesErrorMessageView&) = delete;
  GlanceablesErrorMessageView& operator=(const GlanceablesErrorMessageView&) =
      delete;
  ~GlanceablesErrorMessageView() override = default;

  // Updates the error message view to display proportionally to the given
  // `container_bounds`.
  void UpdateBoundsToContainer(const gfx::Rect& container_bounds);

  std::u16string GetMessageForTest() const;
  views::LabelButton* GetButtonForTest() const { return action_button_; }

 private:
  raw_ptr<views::Label> error_message_label_ = nullptr;
  raw_ptr<views::LabelButton> action_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_ERROR_MESSAGE_VIEW_H_

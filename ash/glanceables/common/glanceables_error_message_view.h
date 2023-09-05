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
 public:
  METADATA_HEADER(GlanceablesErrorMessageView);

  GlanceablesErrorMessageView(views::Button::PressedCallback callback,
                              const std::u16string& error_message);
  GlanceablesErrorMessageView(const GlanceablesErrorMessageView&) = delete;
  GlanceablesErrorMessageView& operator=(const GlanceablesErrorMessageView&) =
      delete;
  ~GlanceablesErrorMessageView() override = default;

  // Updates the error message view to display proportionally to the given
  // `container_bounds`.
  void UpdateBoundsToContainer(const gfx::Rect& container_bounds);

 private:
  raw_ptr<views::Label> error_message_label_ = nullptr;
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_ERROR_MESSAGE_VIEW_H_

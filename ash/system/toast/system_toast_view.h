// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class ScopedA11yOverrideWindowSetter;
class SystemShadow;

// The System Toast view. (go/toast-style-spec)
// This view supports different configurations depending on the provided
// toast data parameters. It will always have a body text, and may have a
// leading icon and a trailing button.
class ASH_EXPORT SystemToastView : public views::FlexLayoutView {
  METADATA_HEADER(SystemToastView, views::FlexLayoutView)

 public:
  SystemToastView(const std::u16string& text,
                  const std::u16string& dismiss_text = std::u16string(),
                  base::RepeatingClosure dismiss_callback = base::DoNothing(),
                  const gfx::VectorIcon* leading_icon = &gfx::kNoneIcon,
                  bool use_custom_focus = true);
  SystemToastView(const SystemToastView&) = delete;
  SystemToastView& operator=(const SystemToastView&) = delete;
  ~SystemToastView() override;

  bool is_dismiss_button_highlighted() const {
    return is_dismiss_button_highlighted_;
  }

  views::LabelButton* dismiss_button() { return dismiss_button_; }

  // Updates the toast label text.
  void SetText(const std::u16string& text);

  // Toggles the dismiss button's focus. This function is necessary since toasts
  // are not directly focus accessible by tab traversal. This function should
  // only be called if `use_custom_focus` is true.
  // TODO(http://b/325335020): Remove this function once the new overview focus
  // is enabled.
  void ToggleButtonA11yFocus();

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;
  std::unique_ptr<SystemShadow> shadow_;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Used to a11y focus and draw a focus ring on the dismiss button directly
  // through `scoped_a11y_overrider_`.
  bool is_dismiss_button_highlighted_ = false;

  // True if the client controls the focus ring of this view. If this is false,
  // default views focus is used.
  const bool use_custom_focus_;

  // Updates the current a11y override window when the dismiss button is being
  // highlighted.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

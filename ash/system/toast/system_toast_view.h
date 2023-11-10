// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class LabelButton;
}  // namespace views

namespace ash {

struct ToastData;
class ScopedA11yOverrideWindowSetter;
class SystemShadow;

// The System Toast view. (go/toast-style-spec)
// This view supports different configurations depending on the provided
// toast data parameters. It will always have a body text, and may have a
// leading icon and a trailing button.
class ASH_EXPORT SystemToastView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(SystemToastView);

  explicit SystemToastView(const ToastData& toast_data);
  SystemToastView(const SystemToastView&) = delete;
  SystemToastView& operator=(const SystemToastView&) = delete;
  ~SystemToastView() override;

  bool is_dismiss_button_highlighted() const {
    return is_dismiss_button_highlighted_;
  }

  views::LabelButton* dismiss_button() const { return dismiss_button_; }

  // Returns true if there's a button and it was highlighted for accessibility.
  bool ToggleA11yFocus();

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  bool is_dismiss_button_highlighted_ = false;

  // Updates the current a11y override window when the dismiss button is being
  // highlighted.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

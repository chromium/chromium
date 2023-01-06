// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TOAST_STYLE_H_
#define ASH_STYLE_SYSTEM_TOAST_STYLE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class ScopedA11yOverrideWindowSetter;
class SystemShadow;

// A view that has rounded corner with label and button inside. The label shows
// the information. The button inside is optional and has certain functionality
// e.g. dismiss the view or retry. A managed icon will be put ahead of the label
// inside if `is_managed` is true.
// TODO(crbug/1289478): Migrate the `managed_icon_` to `Quick settings toast`,
// which includes an icon on the left.
class ASH_EXPORT SystemToastStyle : public views::View {
 public:
  METADATA_HEADER(SystemToastStyle);

  SystemToastStyle(base::RepeatingClosure dismiss_callback,
                   const std::u16string& text,
                   const std::u16string& dismiss_text,
                   const bool is_managed);
  SystemToastStyle(const SystemToastStyle&) = delete;
  SystemToastStyle& operator=(const SystemToastStyle&) = delete;
  ~SystemToastStyle() override;

  bool is_dismiss_button_highlighted() const {
    return is_dismiss_button_highlighted_;
  }

  // Returns true if the toast has a dismiss button and was highlighted for
  // accessibility, false otherwise.
  bool ToggleA11yFocus();

  // Updates the toast label text.
  void SetText(const std::u16string& text);

  views::LabelButton* button() const { return button_; }

 private:
  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  views::Label* label_ = nullptr;
  views::LabelButton* button_ = nullptr;
  views::ImageView* managed_icon_ = nullptr;

  // Tells the toast if the dismiss button is already highlighted if one exists.
  bool is_dismiss_button_highlighted_ = false;

  std::unique_ptr<SystemShadow> shadow_;

  // Updates the current a11y override window when the dismiss button is being
  // highlighted.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TOAST_STYLE_H_

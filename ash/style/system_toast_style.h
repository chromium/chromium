// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TOAST_STYLE_H_
#define ASH_STYLE_SYSTEM_TOAST_STYLE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class ScopedA11yOverrideWindowSetter;
class SystemShadow;

// A view used to present Toasts. It has rounded corners and may have a
// dismiss button if a `dismiss_text` is provided, and a `leading_icon` if one
// is provided. The spacing surrounding the elements may change if the text has
// one or two lines.
class ASH_EXPORT SystemToastStyle : public views::View {
 public:
  METADATA_HEADER(SystemToastStyle);

  SystemToastStyle(base::RepeatingClosure dismiss_callback,
                   const std::u16string& text,
                   const std::u16string& dismiss_text,
                   const gfx::VectorIcon& leading_icon = gfx::kNoneIcon);
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
  friend class ToastManagerImplTest;

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  const gfx::VectorIcon* const leading_icon_ = nullptr;

  // Owned by views hierarchy.
  views::Label* label_ = nullptr;
  views::LabelButton* button_ = nullptr;
  views::ImageView* leading_icon_view_ = nullptr;

  // Tells the toast if the dismiss button is already highlighted if one exists.
  bool is_dismiss_button_highlighted_ = false;

  std::unique_ptr<SystemShadow> shadow_;

  // Updates the current a11y override window when the dismiss button is being
  // highlighted.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TOAST_STYLE_H_

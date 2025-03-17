// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

#include <string_view>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Button;
class Label;
}  // namespace views

namespace ash {

class SystemShadow;

// The System Toast view. (go/toast-style-spec)
// This view supports different configurations depending on the provided
// toast data parameters. It will always have a body text, and may have a
// leading icon and a button containing text or an icon.
class ASH_EXPORT SystemToastView : public views::FlexLayoutView {
  METADATA_HEADER(SystemToastView, views::FlexLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSystemToastViewElementId);

  // Type of button to show next to the toast's body text.
  enum class ButtonType {
    kNone,
    kTextButton,
    kIconButton,
  };

  SystemToastView(
      const std::u16string& text,
      ButtonType button_type = ButtonType::kNone,
      const std::u16string& button_text = std::u16string(),
      const gfx::VectorIcon* button_icon = &gfx::VectorIcon::EmptyIcon(),
      base::RepeatingClosure button_callback = base::DoNothing(),
      const gfx::VectorIcon* leading_icon = &gfx::VectorIcon::EmptyIcon());
  SystemToastView(const SystemToastView&) = delete;
  SystemToastView& operator=(const SystemToastView&) = delete;
  ~SystemToastView() override;

  views::Button* button() { return button_; }

  // Updates the toast label text.
  void SetText(std::u16string_view text);
  std::u16string_view GetText() const;

 private:
  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> label_ = nullptr;
  // Button which either contains text or an icon depending on the toast's
  // `ButtonType`.
  raw_ptr<views::Button> button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

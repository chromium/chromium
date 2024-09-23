// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class SystemShadow;

// The System Toast view. (go/toast-style-spec)
// This view supports different configurations depending on the provided
// toast data parameters. It will always have a body text, and may have a
// leading icon and a trailing button.
class ASH_EXPORT SystemToastView : public views::FlexLayoutView {
  METADATA_HEADER(SystemToastView, views::FlexLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSystemToastViewElementId);

  SystemToastView(const std::u16string& text,
                  const std::u16string& dismiss_text = std::u16string(),
                  base::RepeatingClosure dismiss_callback = base::DoNothing(),
                  const gfx::VectorIcon* leading_icon = &gfx::kNoneIcon);
  SystemToastView(const SystemToastView&) = delete;
  SystemToastView& operator=(const SystemToastView&) = delete;
  ~SystemToastView() override;

  views::LabelButton* dismiss_button() { return dismiss_button_; }

  // Updates the toast label text.
  void SetText(const std::u16string& text);
  const std::u16string& GetText() const;

 private:
  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

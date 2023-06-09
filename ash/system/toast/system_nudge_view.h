// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class ImageView;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

struct AnchoredNudgeData;

// The System Nudge view. (go/cros-educationalnudge-spec)
// This view supports different configurations depending on the provided
// nudge data parameters. It will always have a body text, and may have a
// leading image view, a title text, and up to two buttons.
class ASH_EXPORT SystemNudgeView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(SystemNudgeView);

  SystemNudgeView(const AnchoredNudgeData& nudge_data);
  SystemNudgeView(const SystemNudgeView&) = delete;
  SystemNudgeView& operator=(const SystemNudgeView&) = delete;
  ~SystemNudgeView() override;

  views::ImageView* image_view() const { return image_view_; }
  views::Label* body_label() const { return body_label_; }
  views::Label* title_label() const { return title_label_; }
  views::LabelButton* dismiss_button() const { return dismiss_button_; }
  views::LabelButton* second_button() const { return second_button_; }

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::Label> body_label_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::LabelButton> dismiss_button_ = nullptr;
  raw_ptr<views::LabelButton> second_button_ = nullptr;

  // Sets the maximum width for `title_label_` and `body_label_`.
  void SetLabelsMaxWidth(int max_width);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_

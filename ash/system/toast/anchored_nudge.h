// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class SystemToastStyle;

// Contents view class for the anchored nudge widget.
// TODO(b/279653685): Accept an `AnchoredNudgeData` parameter to set the view's
// contents.
class ASH_EXPORT AnchoredNudge : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AnchoredNudge);

  explicit AnchoredNudge(views::View* anchor);
  AnchoredNudge(const AnchoredNudge&) = delete;
  AnchoredNudge& operator=(const AnchoredNudge&) = delete;
  ~AnchoredNudge() override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // Update arrow to adjust to items that are anchored to the shelf.
  void UpdateArrowFromShelfAlignment(ShelfAlignment alignment);

 private:
  SystemToastStyle* toast_contents_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_H_

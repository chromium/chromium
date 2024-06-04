// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_
#define ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class PickerPreviewBubbleView;

class ASH_EXPORT PickerPreviewBubbleController : public views::WidgetObserver {
 public:
  PickerPreviewBubbleController();
  PickerPreviewBubbleController(const PickerPreviewBubbleController&) = delete;
  PickerPreviewBubbleController& operator=(
      const PickerPreviewBubbleController&) = delete;
  ~PickerPreviewBubbleController() override;

  // `async_preview_image` must remain alive while the bubble is open.
  // `anchor_view` must not be `nullptr`.
  // Destroying `anchor_view` closes the bubble if it's shown.
  void ShowBubble(HoldingSpaceImage* async_preview_image,
                  views::View* anchor_view);

  // TODO: b/322899032 - Take in an `anchor_view` to avoid accidentally closing
  // the bubble view shown by a different anchor view.
  void CloseBubble();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  PickerPreviewBubbleView* bubble_view_for_testing() const;

 private:
  void UpdateBubbleImage();

  raw_ptr<HoldingSpaceImage> async_preview_image_;

  // Owned by the bubble widget.
  raw_ptr<PickerPreviewBubbleView> bubble_view_;

  base::CallbackListSubscription image_subscription_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_

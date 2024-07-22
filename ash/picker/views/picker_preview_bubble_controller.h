// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_
#define ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_list.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_observer.h"

namespace base {
class FilePath;
}

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

  // Creates the preview bubble if needed and shows it after a delay. If called
  // while a bubble was previously already created, the existing bubble is kept
  // but the delay to show the bubble (if not already shown) is reset.
  // `async_preview_image` must remain alive while the bubble is open.
  // `anchor_view` must not be `nullptr`.
  // Destroying `anchor_view` closes the bubble if it's shown.
  void ShowBubbleAfterDelay(HoldingSpaceImage* async_preview_image,
                            const base::FilePath& path,
                            views::View* anchor_view);

  // TODO: b/322899032 - Take in an `anchor_view` to avoid accidentally closing
  // the bubble view shown by a different anchor view.
  void CloseBubble();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void ShowBubbleImmediatelyForTesting(
      HoldingSpaceImage* async_preview_image,
      base::OnceCallback<std::optional<base::File::Info>()> get_file_info,
      views::View* anchor_view);

  PickerPreviewBubbleView* bubble_view_for_testing() const;

 private:
  void UpdateBubbleImage();
  void UpdateBubbleMetadata(std::optional<base::File::Info> info);

  // `get_file_info` is run in a `base::MayBlock()` task.
  void CreateBubbleWidget(
      HoldingSpaceImage* async_preview_image,
      base::OnceCallback<std::optional<base::File::Info>()> get_file_info,
      views::View* anchor_view);

  // Shows the bubble if one has been created. Does nothing if the bubble is
  // already being shown.
  void ShowBubble();

  // Timer to show the preview bubble after a delay.
  base::OneShotTimer show_bubble_timer_;

  raw_ptr<HoldingSpaceImage> async_preview_image_;

  // Owned by the bubble widget.
  raw_ptr<PickerPreviewBubbleView> bubble_view_;

  base::CallbackListSubscription image_subscription_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<PickerPreviewBubbleController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_CONTROLLER_H_

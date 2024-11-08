// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_WIDGET_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_bubble_event_filter.h"
#include "base/time/time.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

enum class PickerPositionType;
class QuickInsertViewDelegate;

class ASH_EXPORT QuickInsertWidget : public views::Widget {
 public:
  QuickInsertWidget(const QuickInsertWidget&) = delete;
  QuickInsertWidget& operator=(const QuickInsertWidget&) = delete;
  ~QuickInsertWidget() override;

  // `delegate` must remain valid for the lifetime of the created Widget.
  // `anchor_bounds` is in screen coordinates.
  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // Widget to be created. For example, if the feature was triggered by a mouse
  // click, then it should be the timestamp of the click. By default, the
  // timestamp is the time this function is called.
  static views::UniqueWidgetPtr Create(
      QuickInsertViewDelegate* delegate,
      const gfx::Rect& anchor_bounds,
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // Same as `Create`, except the created QuickInsertWidget tries to position
  // itself at the center of the display containing `anchor_bounds`.
  // `anchor_bounds` is in screen coordinates.
  static views::UniqueWidgetPtr CreateCentered(
      QuickInsertViewDelegate* delegate,
      const gfx::Rect& anchor_bounds,
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // views::Widget:
  void OnNativeBlur() override;

 private:
  explicit QuickInsertWidget(QuickInsertViewDelegate* delegate,
                             const gfx::Rect& anchor_bounds,
                             PickerPositionType position_type,
                             base::TimeTicks trigger_event_timestamp);

  // Used to close the Quick Insert widget when the user clicks outside of it.
  PickerBubbleEventFilter bubble_event_filter_;

  raw_ptr<QuickInsertViewDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_WIDGET_H_

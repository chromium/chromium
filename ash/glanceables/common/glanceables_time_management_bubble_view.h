// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_

#include "ash/glanceables/common/glanceables_error_message_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace ash {

// Glanceables Time Management bubble container that is a child of
// `GlanceableTrayChildBubble`.
class GlanceablesTimeManagementBubbleView : public views::FlexLayoutView {
  METADATA_HEADER(GlanceablesTimeManagementBubbleView, views::FlexLayoutView)

 public:
  // The attribute that describes what type this view is used for.
  enum class Context { kTasks, kClassroom };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnExpandStateChanged(Context context, bool is_expanded) = 0;
  };

  GlanceablesTimeManagementBubbleView();
  GlanceablesTimeManagementBubbleView(
      const GlanceablesTimeManagementBubbleView&) = delete;
  GlanceablesTimeManagementBubbleView& operator=(
      const GlanceablesTimeManagementBubbleView&) = delete;
  ~GlanceablesTimeManagementBubbleView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void Layout(PassKey) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Removes an active `error_message_` from the view, if any.
  void MaybeDismissErrorMessage();
  void ShowErrorMessage(const std::u16string& error_message,
                        views::Button::PressedCallback callback,
                        GlanceablesErrorMessageView::ButtonActionType type);

  GlanceablesErrorMessageView* error_message() { return error_message_; }

  base::ObserverList<Observer> observers_;

 private:
  // Owned by views hierarchy.
  raw_ptr<GlanceablesErrorMessageView> error_message_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_

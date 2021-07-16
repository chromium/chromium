// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_
#define ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class AppListBubbleEventFilter;
class AppListBubbleView;
class AppListControllerImpl;

// Manages the UI for the bubble launcher used in clamshell mode. Handles
// showing and hiding the UI. Only one bubble can be visible at a time, across
// all displays.
class ASH_EXPORT AppListBubblePresenter : public views::WidgetObserver {
 public:
  explicit AppListBubblePresenter(AppListControllerImpl* controller);
  AppListBubblePresenter(const AppListBubblePresenter&) = delete;
  AppListBubblePresenter& operator=(const AppListBubblePresenter&) = delete;
  ~AppListBubblePresenter() override;

  // Shows the bubble on the display with `display_id`.
  void Show(int64_t display_id);

  // Shows or hides the bubble on the display with `display_id`. Returns the
  // appropriate ShelfAction to indicate whether the bubble was shown or hidden.
  ShelfAction Toggle(int64_t display_id);

  // Closes and destroys the bubble.
  void Dismiss();

  // Returns true if the bubble is showing on any display.
  bool IsShowing() const;

  // Returns true if the assistant page is showing.
  bool IsShowingEmbeddedAssistantUI() const;

  // Switches to the assistant page. Requires the bubble to be open.
  void ShowEmbeddedAssistantUI();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  views::Widget* bubble_widget_for_test() { return bubble_widget_; }
  AppListBubbleView* bubble_view_for_test() { return bubble_view_; }

 private:
  // Callback for AppListBubbleEventFilter, used to notify this of presses
  // outside the bubble.
  void OnPressOutsideBubble();

  // Gets the display id for the display `bubble_widget_` is shown on, returns
  // kInvalidDisplayId if not shown.
  int64_t GetDisplayId() const;

  AppListControllerImpl* const controller_;

  // Owned by native widget.
  views::Widget* bubble_widget_ = nullptr;

  // Owned by views.
  AppListBubbleView* bubble_view_ = nullptr;

  // Closes the widget when the user clicks outside of it.
  std::unique_ptr<AppListBubbleEventFilter> bubble_event_filter_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_

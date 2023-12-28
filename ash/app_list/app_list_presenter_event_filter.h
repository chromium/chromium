// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_EVENT_FILTER_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {

class AppListControllerImpl;
class AppListPresenterImpl;
class AppListView;

// Listens for mouse clicks and taps outside the app list to close the UI when
// necessary. Used by the fullscreen launcher
class ASH_EXPORT AppListPresenterEventFilter : public ui::EventHandler {
 public:
  AppListPresenterEventFilter(AppListControllerImpl* controller,
                              AppListPresenterImpl* presenter,
                              AppListView* view);
  AppListPresenterEventFilter(const AppListPresenterEventFilter&) = delete;
  AppListPresenterEventFilter& operator=(const AppListPresenterEventFilter&) =
      delete;
  ~AppListPresenterEventFilter() override;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void ProcessLocatedEvent(ui::LocatedEvent* event);

  const raw_ptr<AppListControllerImpl> controller_;
  const raw_ptr<AppListPresenterImpl> presenter_;
  const raw_ptr<AppListView> view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_EVENT_FILTER_H_

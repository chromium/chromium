// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_EVENT_TARGETER_H_
#define ASH_APP_LIST_APP_LIST_EVENT_TARGETER_H_

#include "ui/aura/window_targeter.h"

namespace ash {

class AppListViewDelegate;

// This targeter prevents routing events to sub-windows, such as
// RenderHostWindow in order to handle events in context of app list.
// See https://crbug.com/924624
class AppListEventTargeter : public aura::WindowTargeter {
 public:
  explicit AppListEventTargeter(AppListViewDelegate* delegate);
  AppListEventTargeter(const AppListEventTargeter&) = delete;
  AppListEventTargeter& operator=(const AppListEventTargeter&) = delete;
  ~AppListEventTargeter() override;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override;

 private:
  AppListViewDelegate* const delegate_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_EVENT_TARGETER_H_

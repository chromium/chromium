// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_MODE_INDICATOR_OBSERVER_H_
#define ASH_IME_MODE_INDICATOR_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

// The observer for the mode indicator widget so that the widget can be
// closed immediately when a new mode indicator view tries to show before
// the fade out animation completes.
class ModeIndicatorObserver : public views::WidgetObserver {
 public:
  ModeIndicatorObserver();

  ModeIndicatorObserver(const ModeIndicatorObserver&) = delete;
  ModeIndicatorObserver& operator=(const ModeIndicatorObserver&) = delete;

  ~ModeIndicatorObserver() override;

  void AddModeIndicatorWidget(views::Widget* widget);

  // Exposes the active widget for testability.
  views::Widget* active_widget() const { return active_widget_; }

  // views::WidgetObserver override:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  raw_ptr<views::Widget> active_widget_;
};

}  // namespace ash

#endif  // ASH_IME_MODE_INDICATOR_OBSERVER_H_

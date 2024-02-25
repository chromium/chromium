// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TIME_TO_CLICK_RECORDER_H_
#define ASH_SYSTEM_TRAY_TIME_TO_CLICK_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// An event handler that will be installed as PreTargetHandler of |target_view|
// to record TimeToClick metrics.
class TimeToClickRecorder : public ui::EventHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Record TimeToClick metrics. Called by TimeToClickRecorder which is a
    // PreTargetHandler of |target_view|.
    virtual void RecordTimeToClick() = 0;
  };

  TimeToClickRecorder(Delegate* delegate, views::View* target_view);

  TimeToClickRecorder(const TimeToClickRecorder&) = delete;
  TimeToClickRecorder& operator=(const TimeToClickRecorder&) = delete;

  ~TimeToClickRecorder() override = default;

 private:
  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  const raw_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TIME_TO_CLICK_RECORDER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TEST_EVENT_RECORDER_H_
#define ASH_ACCESSIBILITY_TEST_EVENT_RECORDER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter that records events for test purposes.
class ASH_EXPORT TestEventRecorder : public ui::EventRewriter {
 public:
  TestEventRecorder();
  TestEventRecorder(const TestEventRecorder&) = delete;
  TestEventRecorder& operator=(const TestEventRecorder&) = delete;
  ~TestEventRecorder() override;

  const std::vector<std::unique_ptr<ui::Event>>& events() { return events_; }
  ui::EventType last_recorded_event_type() { return events_.back()->type(); }
  size_t recorded_event_count() { return events_.size(); }

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  std::vector<std::unique_ptr<ui::Event>> events_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_TEST_EVENT_RECORDER_H_

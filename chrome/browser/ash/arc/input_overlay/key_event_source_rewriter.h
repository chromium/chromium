// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/events/event_rewriter.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class EventSource;
}  // namespace ui

namespace arc::input_overlay {
// KeyEventSourceRewriter forwards the key event from primary root window to
// the extended root window event source when the input-overlay enabled window
// is on the extended display.
class KeyEventSourceRewriter : public ui::EventRewriter {
 public:
  explicit KeyEventSourceRewriter(aura::Window* top_level_window);
  KeyEventSourceRewriter(const KeyEventSourceRewriter&) = delete;
  KeyEventSourceRewriter& operator=(const KeyEventSourceRewriter&) = delete;
  ~KeyEventSourceRewriter() override;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  base::ScopedObservation<ui::EventSource, ui::EventRewriter> observation_{
      this};

  raw_ptr<aura::Window> top_level_window_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_

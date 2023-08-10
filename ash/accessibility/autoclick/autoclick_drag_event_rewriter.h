// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_
#define ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter to change move events into drag events during drag-and-drop
// actions from Autoclick.
class ASH_EXPORT AutoclickDragEventRewriter : public ui::EventRewriter {
 public:
  AutoclickDragEventRewriter() = default;

  AutoclickDragEventRewriter(const AutoclickDragEventRewriter&) = delete;
  AutoclickDragEventRewriter& operator=(const AutoclickDragEventRewriter&) =
      delete;

  ~AutoclickDragEventRewriter() override = default;

  void SetEnabled(bool enabled);
  bool IsEnabled() const;

  // ui::EventRewriter (visible for testing):
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;
  bool SupportsNonRootLocation() const override;

 private:
  bool enabled_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_

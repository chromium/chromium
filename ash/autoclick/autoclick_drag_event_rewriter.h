// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_
#define ASH_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter to change move events into drag events during drag-and-drop
// actions from Autoclick.
class ASH_EXPORT AutoclickDragEventRewriter : public ui::EventRewriter {
 public:
  AutoclickDragEventRewriter() = default;
  ~AutoclickDragEventRewriter() override = default;

  void SetEnabled(bool enabled);
  bool IsEnabled() const;

  // ui::EventRewriter (visible for testing):
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override;

 private:
  // ui::EventRewriter:
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  bool enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(AutoclickDragEventRewriter);
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_AUTOCLICK_DRAG_EVENT_REWRITER_H_

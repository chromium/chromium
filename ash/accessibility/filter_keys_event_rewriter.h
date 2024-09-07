// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_
#define ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter that delays or cancels some keyboard events.
class ASH_EXPORT FilterKeysEventRewriter : public ui::EventRewriter {
 public:
  FilterKeysEventRewriter();
  FilterKeysEventRewriter(const FilterKeysEventRewriter&) = delete;
  FilterKeysEventRewriter& operator=(const FilterKeysEventRewriter&) = delete;
  ~FilterKeysEventRewriter() override;

  void SetBounceKeysEnabled(bool enabled);
  bool IsBounceKeysEnabled();

 private:
  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  bool bounce_keys_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_

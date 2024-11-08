// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_
#define ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/constants/ash_constants.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
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
  bool IsBounceKeysEnabled() const;

  void SetBounceKeysDelay(base::TimeDelta delay);
  const base::TimeDelta& GetBounceKeysDelay() const;

 private:
  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // Returns true if a key entry was erased.
  bool EraseNextKeyReleaseToDiscard(ui::DomCode dom_code);

  // Resets bounce keys state, used when disabling bounce keys functionality.
  void ResetBounceKeysState();

  // Enables bounce keys functionality.
  bool bounce_keys_enabled_ = false;
  // Delay until subsequent key strokes are accepted for bounce keys.
  base::TimeDelta bounce_keys_delay_ = kDefaultAccessibilityBounceKeysDelay;

  // The last key that had a key pressed event.
  // Used to reset bounce keys delay when a different key is pressed.
  std::optional<ui::DomCode> last_pressed_key_;
  // The time of the last key released event.
  // Used to calculate whether bounce keys delay has passed.
  std::optional<base::TimeTicks> last_released_time_;
  // Set of keys where the key released event should be discarded.
  // Used to pair key released events with discarded key pressed events,
  // e.g. for when key press and release events cross the delay boundary.
  base::flat_set<ui::DomCode> next_key_release_to_discard_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_FILTER_KEYS_EVENT_REWRITER_H_

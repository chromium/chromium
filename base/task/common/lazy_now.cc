// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/lazy_now.h"

#include <optional>

#include "base/check.h"
#include "base/time/tick_clock.h"

namespace base {

LazyNow::LazyNow(TimeTicks now) : now_(now), tick_clock_(nullptr) {}

LazyNow::LazyNow(std::optional<TimeTicks> now, const TickClock* tick_clock)
    : now_(now), tick_clock_(tick_clock) {
  DCHECK(tick_clock);
}

LazyNow::LazyNow(const TickClock* tick_clock) : tick_clock_(tick_clock) {
  DCHECK(tick_clock);
}

LazyNow::LazyNow(LazyNow&& move_from) noexcept
    : now_(move_from.now_), tick_clock_(move_from.tick_clock_) {
  move_from.tick_clock_ = nullptr;
  move_from.now_ = std::nullopt;
}

TimeTicks LazyNow::Now() {
  // It looks tempting to avoid using Optional and to rely on is_null() instead,
  // but in some test environments clock intentionally starts from zero.
  if (!now_) {
    DCHECK(tick_clock_);  // It can fire only on use after std::move.
    now_ = tick_clock_->NowTicks();
  }
  return *now_;
}

}  // namespace base

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/check_op.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

// static
ScrollJankV4Frame::BeginFrameArgsForScrollJank
ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(
    const viz::BeginFrameArgs& args,
    uint64_t result_id) {
  return {.frame_time = args.frame_time,
          .interval = args.interval,
          .result_id = result_id};
}

// static
ScrollJankV4Frame::BeginFrameArgsForScrollJank
ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(
    const ScrollEventMetrics::DispatchBeginFrameArgs& args,
    uint64_t result_id) {
  return {.frame_time = args.frame_time,
          .interval = args.interval,
          .result_id = result_id};
}

ScrollJankV4Frame::ScrollJankV4Frame(BeginFrameArgsForScrollJank args,
                                     ScrollDamage damage,
                                     StageList stages)
    : args(args), damage(damage), stages(stages) {}

ScrollJankV4Frame::ScrollJankV4Frame(const ScrollJankV4Frame& frame) = default;

ScrollJankV4Frame::~ScrollJankV4Frame() = default;

ScrollJankV4Frame::Stage::ScrollUpdates::ScrollUpdates(
    std::optional<Real> real,
    std::optional<Synthetic> synthetic,
    std::optional<base::TimeTicks> scroll_begin_arrival_timestamp)
    : real_(std::move(real)),
      synthetic_(std::move(synthetic)),
      scroll_begin_arrival_timestamp_(scroll_begin_arrival_timestamp) {
  CHECK(real_.has_value() || synthetic_.has_value());
}

ScrollJankV4Frame::Stage::Stage(
    std::variant<ScrollStart, ScrollUpdates, ScrollEnd> stage)
    : stage(std::move(stage)) {}

ScrollJankV4Frame::Stage::Stage(const Stage& stage) = default;

ScrollJankV4Frame::Stage::~Stage() = default;

}  // namespace cc

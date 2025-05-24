// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_context.h"

#include <utility>

namespace cc {

AppendQuadsContext::AppendQuadsContext() = default;

AppendQuadsContext::AppendQuadsContext(
    DrawMode draw_mode,
    base::flat_set<blink::ViewTransitionToken> capture_view_transition_tokens,
    bool for_view_transition_capture)
    : draw_mode(draw_mode),
      capture_view_transition_tokens(std::move(capture_view_transition_tokens)),
      for_view_transition_capture(for_view_transition_capture) {}

AppendQuadsContext::~AppendQuadsContext() = default;

}  // namespace cc

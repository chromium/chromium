// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_CONTEXT_H_
#define CC_LAYERS_APPEND_QUADS_CONTEXT_H_

#include "base/containers/flat_set.h"
#include "cc/cc_export.h"
#include "cc/layers/draw_mode.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace cc {

struct CC_EXPORT AppendQuadsContext {
  AppendQuadsContext();
  AppendQuadsContext(
      DrawMode draw_mode,
      base::flat_set<blink::ViewTransitionToken> capture_view_transition_tokens,
      bool for_view_transition_capture);
  ~AppendQuadsContext();

  // The draw mode used.
  DrawMode draw_mode = DRAW_MODE_NONE;

  // A set of all view transition tokens in the capture phase.
  // During view transition capture phase, the transitions that are in this
  // state (as indicated by these tokens) are treated differently from other
  // view transitions. Specifically, the capture phase generates two render
  // passes -- one for capture which isn't displayed and one for display. The
  // reason this is important is that the captured render passes are filtered
  // to exclude ancestor clips as well as nested view transiiton elements,
  // which may not be appropriate for display. Note that this is only set if
  // ViewTransitionCaptureAndDisplay feature is enabled.
  base::flat_set<blink::ViewTransitionToken> capture_view_transition_tokens;

  // In separate render pass appends, this indicates whether we're appending for
  // view transition capture or for display
  bool for_view_transition_capture = false;
};

}  // namespace cc

#endif  // CC_LAYERS_APPEND_QUADS_CONTEXT_H_

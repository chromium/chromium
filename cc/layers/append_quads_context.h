// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_CONTEXT_H_
#define CC_LAYERS_APPEND_QUADS_CONTEXT_H_

#include "cc/cc_export.h"
#include "cc/layers/draw_mode.h"

namespace cc {

struct CC_EXPORT AppendQuadsContext {
  DrawMode draw_mode = DRAW_MODE_NONE;
};

}  // namespace cc

#endif  // CC_LAYERS_APPEND_QUADS_CONTEXT_H_

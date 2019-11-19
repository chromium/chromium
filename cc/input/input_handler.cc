// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/input_handler.h"

namespace cc {

InputHandlerScrollResult::InputHandlerScrollResult()
    : did_scroll(false), did_overscroll_root(false) {
}

InputHandlerPointerResult::InputHandlerPointerResult()
    : type(kUnhandled),
      scroll_units(ui::input_types::ScrollGranularity::kScrollByPrecisePixel) {}

}  // namespace cc

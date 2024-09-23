// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/paint_op_helper.h"
#include "cc/paint/paint_op.h"

namespace cc {

void PrintTo(const PaintOp& op, std::ostream* os) {
  *os << PaintOpHelper::ToString(&op);
}

}  // namespace cc

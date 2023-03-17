// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/test_glanceables_delegate.h"

namespace ash {

TestGlanceablesDelegate::TestGlanceablesDelegate() = default;

TestGlanceablesDelegate::~TestGlanceablesDelegate() = default;

void TestGlanceablesDelegate::OnGlanceablesClosed() {
  ++on_glanceables_closed_count_;
}

}  // namespace ash

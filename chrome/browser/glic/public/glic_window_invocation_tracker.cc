// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_window_invocation_tracker.h"

namespace glic {

GlicWindowInvocationTracker::GlicWindowInvocationTracker()
    : GlicCuiTracker("Glic.CUI.WindowEntryPointInvocation") {}

GlicWindowInvocationTracker::~GlicWindowInvocationTracker() = default;

}  // namespace glic

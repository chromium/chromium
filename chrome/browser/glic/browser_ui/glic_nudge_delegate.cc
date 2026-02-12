// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"

namespace glic {

// Default implementation for Destructor.
GlicNudgeDelegate::~GlicNudgeDelegate() = default;

// Default implementation for GetIsShowingNudge.
bool GlicNudgeDelegate::GetIsShowingGlicNudge() {
  // Default behavior is to return false.
  return false;
}

}  // namespace glic

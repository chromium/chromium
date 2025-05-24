// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/scoped_glic_button_indicator.h"

#include "chrome/browser/ui/views/tabs/glic_button.h"

namespace glic {
ScopedGlicButtonIndicator::ScopedGlicButtonIndicator(GlicButton* glic_button)
    : glic_button_(glic_button) {
  glic_button_->SetDropToAttachIndicator(true);
}

ScopedGlicButtonIndicator::~ScopedGlicButtonIndicator() {
  glic_button_->SetDropToAttachIndicator(false);
}

}  // namespace glic

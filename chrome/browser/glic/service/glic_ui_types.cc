// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_ui_types.h"

#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_widget.h"

namespace glic {

// static
FloatingShowOptions FloatingShowOptions::From(
    BrowserWindowInterface* anchor_browser) {
  return {GlicWidget::GetInitialBounds(anchor_browser,
                                       GlicFloatingUi::GetDefaultSize())};
}

}  // namespace glic

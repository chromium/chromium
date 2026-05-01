// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_CONVERSIONS_H_
#define CHROME_BROWSER_GLIC_WIDGET_CONVERSIONS_H_

#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_types.h"

namespace glic {

// Converts instance show options to side panel coordinator show options.
// Crashes if passed an options object with floating embedder options.
GlicSidePanelCoordinator::ShowOptions ConvertToCoordinatorShowOptions(
    const ShowOptions& options,
    bool supports_peek);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_CONVERSIONS_H_

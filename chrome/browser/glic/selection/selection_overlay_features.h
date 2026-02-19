// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_FEATURES_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_FEATURES_H_

#include "base/feature_list.h"

namespace glic::features {

// Enable new new region selection code path based on Lens overlay controller.
BASE_DECLARE_FEATURE(kGlicRegionSelectionNew);

}  // namespace glic::features

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_FEATURES_H_

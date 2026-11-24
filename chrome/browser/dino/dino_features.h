// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_FEATURES_H_
#define CHROME_BROWSER_DINO_DINO_FEATURES_H_

#include "base/feature_list.h"

namespace dino {

// Master feature flag for the Delight for Desktop Growth (Dino) experiment,
// gating the offline game-over bookmark IPH and associated metrics.
BASE_DECLARE_FEATURE(kDinoDelight);

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_FEATURES_H_

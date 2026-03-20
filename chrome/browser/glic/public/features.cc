// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"

namespace features {

BASE_FEATURE(kGlicTabRestoration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicChromeStatusIcon, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicChromeStatusIconSizePx{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-size-px", 20};

BASE_FEATURE(kGlicOrphanedReattachment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

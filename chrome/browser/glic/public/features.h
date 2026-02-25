// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
#define CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kGlicTabRestoration);
BASE_DECLARE_FEATURE(kGlicDaisyChainViaCoordinator);
BASE_DECLARE_FEATURE(kGlicChromeStatusIcon);

BASE_DECLARE_FEATURE(kGlicOrphanedReattachment);

BASE_DECLARE_FEATURE(kAutoOpenGlicForPdf);

}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_

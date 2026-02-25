// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"

#include "build/build_config.h"

namespace features {

BASE_FEATURE(kGlicTabRestoration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicChromeStatusIcon, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicOrphanedReattachment, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

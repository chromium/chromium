// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"

namespace features {

// Forces all eligible prerenders to be done in an isolated manner such that no
// user-identifying information is used during the prefetch.
BASE_FEATURE(kIsolatePrerenders,
             "IsolatePrerenders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces Chrome to probe the origin before reusing a cached response.
BASE_FEATURE(kIsolatePrerendersMustProbeOrigin,
             "IsolatePrerendersMustProbeOrigin",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

}  // namespace features

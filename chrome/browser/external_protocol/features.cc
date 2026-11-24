// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/features.h"

namespace features {

BASE_FEATURE(kLocalOnlyAppProtocolPrefix, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kLocalOnlyAppProtocolWindows, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kXdgSchemeHandlerDetection, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features

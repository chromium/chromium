// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/tracing_features.h"

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWindowsSystemTracing,
             "WindowsSystemTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

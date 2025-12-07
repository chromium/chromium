// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_TRACING_FEATURES_H_
#define CHROME_BROWSER_TRACING_TRACING_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
// A Feature to selectively enable connecting to the Windows system tracing
// service when the tracing service is started.
BASE_DECLARE_FEATURE(kWindowsSystemTracing);
#endif  // BUILDFLAG(IS_WIN)

#endif  // CHROME_BROWSER_TRACING_TRACING_FEATURES_H_

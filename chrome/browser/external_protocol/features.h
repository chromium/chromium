// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// Blocks web-initiated external protocol launches with a local+ scheme.
// See crbug.com/539667062.
BASE_DECLARE_FEATURE(kLocalOnlyAppProtocolPrefix);

#if BUILDFLAG(IS_LINUX)
// Classifies omnibox input with a scheme that has an XDG "x-scheme-handler"
// association as a URL instead of a search. See crbug.com/40428807.
BASE_DECLARE_FEATURE(kXdgSchemeHandlerDetection);
#endif

#if BUILDFLAG(IS_WIN)
// Blocks web-initiated external protocols marked LocalOnly on Windows.
// See crbug.com/539654408.
BASE_DECLARE_FEATURE(kLocalOnlyAppProtocolWindows);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace features

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_FEATURES_H_

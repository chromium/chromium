// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_FEATURES_H_
#define CHROME_BROWSER_TTC_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "components/ttc/features.h"  // IWYU pragma: export

namespace ttc {

// Enables the first run experience feature for TTC. While FRE is under
// development, this is used to hide the unfinished FRE. Once FRE is complete,
// this can be used to suppress the feature in tests.
BASE_DECLARE_FEATURE(kTtcFre);

// Enables chrome://ttc-internals, which records TTC sessions in memory
// (pipeline health, timeline, tool calls, problems and audio) for debugging.
BASE_DECLARE_FEATURE(kTtcDebugTools);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_FEATURES_H_

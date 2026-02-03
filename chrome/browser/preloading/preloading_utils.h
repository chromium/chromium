// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRELOADING_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRELOADING_UTILS_H_

// This file is used to manage some static functions and constants for
// preloading. Some typical cases can be:
// * Indicates whether a preloading-related feature is enabled.
// * Stores the constants to avoid hardcoded strings.
namespace preloading_utils {

// LINT.IfChange(PreloadingEmbedderTriggerType)
extern const char kBookmarkBarMetricSuffix[];
extern const char kNewTabPageMetricSuffix[];
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/histograms.xml:PagePreloadingTriggerType,
// //tools/metrics/histograms/metadata/page/histograms.xml:PagePreloadingTriggerType,
// //tools/metrics/histograms/metadata/prefetch/histograms.xml:TriggerTypeAndEagerness)

}  // namespace preloading_utils

#endif  // CHROME_BROWSER_PRELOADING_PRELOADING_UTILS_H_

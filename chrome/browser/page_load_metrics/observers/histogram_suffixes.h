// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HISTOGRAM_SUFFIXES_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HISTOGRAM_SUFFIXES_H_

namespace internal {

// Histogram suffixes used by multiple observers.
extern const char kHistogramDOMContentLoadedEventFiredBackgroundSuffix[];
extern const char kHistogramDOMContentLoadedEventFiredSuffix[];
extern const char kHistogramFirstContentfulPaintSuffix[];
extern const char kHistogramFirstImagePaintSuffix[];
extern const char kHistogramFirstInputDelaySuffix[];
extern const char kHistogramFirstMeaningfulPaintSuffix[];
extern const char kHistogramFirstPaintSuffix[];
extern const char kHistogramForegroundToFirstContentfulPaintSuffix[];
extern const char kHistogramLargestContentfulPaintSuffix[];
extern const char kHistogramLoadEventFiredBackgroundSuffix[];
extern const char kHistogramLoadEventFiredSuffix[];
extern const char kHistogramParseBlockedOnScriptLoadSuffix[];
extern const char kHistogramParseDurationSuffix[];
extern const char kHistogramParseStartSuffix[];

extern const char kSuffixFirstNavigation[];
extern const char kSuffixSubsequentNavigation[];
extern const char kSuffixFromNewTabPage[];

extern const char kSuffixIsBrowserStarting[];

}  // namespace internal

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HISTOGRAM_SUFFIXES_H_

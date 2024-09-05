// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"

namespace internal {

const char kHistogramDOMContentLoadedEventFiredBackgroundSuffix[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired.Background";
const char kHistogramDOMContentLoadedEventFiredSuffix[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kHistogramFirstContentfulPaintSuffix[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramFirstImagePaintSuffix[] =
    "PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFirstInputDelaySuffix[] =
    "InteractiveTiming.FirstInputDelay4";
const char kHistogramFirstMeaningfulPaintSuffix[] =
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kHistogramFirstPaintSuffix[] = "PaintTiming.NavigationToFirstPaint";
const char kHistogramForegroundToFirstContentfulPaintSuffix[] =
    "PaintTiming.ForegroundToFirstContentfulPaint";
const char kHistogramLargestContentfulPaintSuffix[] =
    "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramLoadEventFiredBackgroundSuffix[] =
    "DocumentTiming.NavigationToLoadEventFired.Background";
const char kHistogramLoadEventFiredSuffix[] =
    "DocumentTiming.NavigationToLoadEventFired";
const char kHistogramParseBlockedOnScriptLoadSuffix[] =
    "ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramParseDurationSuffix[] = "ParseTiming.ParseDuration";
const char kHistogramParseStartSuffix[] = "ParseTiming.NavigationToParseStart";

const char kSuffixFirstNavigation[] = ".IsFirstNavigation";
const char kSuffixSubsequentNavigation[] = ".IsSubsequentNavigation";
const char kSuffixFromNewTabPage[] = ".FromNewTabPage";
const char kSuffixIsBrowserStarting[] = ".IsBrowserStarting";

}  // namespace internal

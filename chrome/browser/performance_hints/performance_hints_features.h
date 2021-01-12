// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_FEATURES_H_
#define CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_FEATURES_H_

#include <string>

#include "base/feature_list.h"

namespace performance_hints {
namespace features {

// Exposed for chrome://flags.
extern const base::Feature kPageInfoPerformanceHints;

// Exposed for testing.
extern const base::Feature kPerformanceHintsObserver;
extern const base::Feature kPerformanceHintsTreatUnknownAsFast;
extern const base::Feature kPerformanceHintsHandleRewrites;
extern const base::Feature kContextMenuPerformanceInfo;
extern const base::Feature kContextMenuPerformanceInfoAndRemoteHintFetching;

// Returns true if PerformanceHintsObserver should be added as a tab helper and
// fetch performance hints.
bool IsPerformanceHintsObserverEnabled();

// Returns true if hints of PERFORMANCE_UNKNOWN should be overridden to
// PERFORMANCE_FAST.
//
// This does not affect the value that is recorded via UMA.
bool ShouldTreatUnknownAsFast();

// Returns true if PerformanceHintsRewriteHandler should be used to detect
// rewritten URLs and revert them to their original form.
bool ShouldHandleRewrites();

// Returns the config string to be used by PerformanceHintsRewriteHandler.
//
// Contains rewritten URL patterns that should be replaced with a URL contained
// in their query params.
std::string GetRewriteConfigString();

// Returns true if FAST_HOST_HINTS should be checked if available.
bool AreFastHostHintsEnabled();

// Returns true if LINK_PERFORMANCE hints should be requested and used.
bool AreLinkPerformanceHintsEnabled();

// Returns true if performance info should be shown in the context menu.
bool IsContextMenuPerformanceInfoEnabled();

}  // namespace features
}  // namespace performance_hints

#endif  // CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_FEATURES_H_

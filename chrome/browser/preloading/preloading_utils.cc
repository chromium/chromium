// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_utils.h"

namespace preloading_utils {

// If you add a new type of preloading trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type.
// LINT.IfChange
const char kBookmarkBarMetricSuffix[] = "BookmarkBar";
const char kNewTabPageMetricSuffix[] = "NewTabPage";
// LINT.ThenChange()

}  // namespace preloading_utils

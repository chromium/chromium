// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_utils.h"

namespace prerender_utils {

// If you add a new type of prerender trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type.
// LINT.IfChange
const char kPrewarmDefaultSearchEngineMetricSuffix[] =
    "PrewarmDefaultSearchEngine";
const char kDefaultSearchEngineMetricSuffix[] = "DefaultSearchEngine";
const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";
const char kBookmarkBarMetricSuffix[] = "BookmarkBar";
const char kNewTabPageMetricSuffix[] = "NewTabPage";
const char kLinkPreviewMetricsSuffix[] = "LinkPreview";
// LINT.ThenChange()

}  // namespace prerender_utils

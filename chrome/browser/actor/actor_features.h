// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_
#define CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

namespace actor {

BASE_DECLARE_FEATURE(kGlicActionAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlist);
BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlistExact);
BASE_DECLARE_FEATURE_PARAM(bool, kAllowlistOnly);

BASE_DECLARE_FEATURE(kGlicActionUseOptimizationGuide);

BASE_DECLARE_FEATURE(kGlicBlockNavigationToDangerousContentTypes);

BASE_DECLARE_FEATURE(kGlicCrossOriginNavigationGating);

// When enabled, the actor will send a dialog request to the web client to
// allow the user to select a credential to use for a site. When disabled, the
// actor will automatically use the first credential.
// TODO(crbug.com/427815202): Remove this once the front end is wired up.
BASE_DECLARE_FEATURE(kGlicEnableAutoLoginDialogs);

// Enables the Paint Preview backend for taking screenshots.
BASE_DECLARE_FEATURE(kGlicTabScreenshotPaintPreviewBackend);

// Enables a full page screenshot to be taken rather than only the viewport.
extern const base::FeatureParam<bool> kFullPageScreenshot;

// Controls the maximum memory/file bytes used for the capture of a single
// frame. 0 means no maximum.
extern const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes;

// Controls whether iframe redaction is enabled, and which scope is used if so.
extern const base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction;

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace actor {

BASE_FEATURE(kGlicActionAllowlist, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kAllowlist,
                   &kGlicActionAllowlist,
                   "allowlist",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAllowlistExact,
                   &kGlicActionAllowlist,
                   "allowlist_exact",
                   "");
BASE_FEATURE_PARAM(bool,
                   kAllowlistOnly,
                   &kGlicActionAllowlist,
                   "allowlist_only",
                   true);

BASE_FEATURE(kGlicActionUseOptimizationGuide, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicBlockNavigationToDangerousContentTypes,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicCrossOriginNavigationGating,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicEnableAutoLoginDialogs, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicTabScreenshotPaintPreviewBackend,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kFullPageScreenshot{
    &kGlicTabScreenshotPaintPreviewBackend, "full_page_screenshot", false};

const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes{
    &kGlicTabScreenshotPaintPreviewBackend, "screenshot_max_per_capture_bytes",
    0};

constexpr base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>::Option
    kScreenshotIframeRedactionOptions[] = {
        {page_content_annotations::ScreenshotIframeRedactionScope::kNone,
         "none"},
        {page_content_annotations::ScreenshotIframeRedactionScope::kCrossSite,
         "cross-site"},
        {page_content_annotations::ScreenshotIframeRedactionScope::kCrossOrigin,
         "cross-origin"},
};

const base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction{
        &kGlicTabScreenshotPaintPreviewBackend, "screenshot_iframe_redaction",
        page_content_annotations::ScreenshotIframeRedactionScope::kCrossSite,
        &kScreenshotIframeRedactionOptions};

}  // namespace actor

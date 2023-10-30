// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"

#include "base/check_is_test.h"
#include "base/test/scoped_feature_list.h"

namespace apps {

void EnableLinkCapturingUXForTesting(
    base::test::ScopedFeatureList& scoped_feature_list) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  scoped_feature_list.InitAndEnableFeature(
      apps::features::kLinkCapturingUiUpdate);
#else
  scoped_feature_list.InitAndEnableFeature(
      apps::features::kDesktopPWAsLinkCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void DisableLinkCapturingUXForTesting(
    base::test::ScopedFeatureList& scoped_feature_list) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  scoped_feature_list.InitAndDisableFeature(
      apps::features::kLinkCapturingUiUpdate);
#else
  scoped_feature_list.InitAndDisableFeature(
      apps::features::kDesktopPWAsLinkCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace apps

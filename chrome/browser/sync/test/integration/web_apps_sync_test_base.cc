// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"

#include "base/containers/extend.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#endif

namespace web_app {

WebAppsSyncTestBase::WebAppsSyncTestBase(TestType test_type)
    : SyncTest(test_type) {
  std::vector<base::test::FeatureRef> enabled_features;

#if !BUILDFLAG(IS_CHROMEOS)
  // TOOD(b/313492499): Update test driver to work with new intent picker UI.
  enabled_features.push_back(features::kPwaNavigationCapturing);
#endif

  scoped_feature_list_.InitWithFeatures(enabled_features, {});
}

WebAppsSyncTestBase::~WebAppsSyncTestBase() = default;

}  // namespace web_app

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chromeos/components/sample_system_web_app_ui/url_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using SampleSystemWebAppIntegrationTest = SystemWebAppIntegrationTest;

// Test that the Sample System Web App installs and launches correctly. Runs
// some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(SampleSystemWebAppIntegrationTest, SampleSystemWebApp) {
  const GURL url(chromeos::kChromeUISampleSystemWebAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::SAMPLE, url, "Sample System Web App"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SampleSystemWebAppIntegrationTest);

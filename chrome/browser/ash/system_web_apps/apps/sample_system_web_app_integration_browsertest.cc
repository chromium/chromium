// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using SampleSystemWebAppIntegrationTest = ash::SystemWebAppIntegrationTest;

// Test that the Sample System Web App installs and launches correctly. Runs
// some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(SampleSystemWebAppIntegrationTest, SampleSystemWebApp) {
  const GURL url(ash::kChromeUISampleSystemWebAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::SAMPLE, url, "Sample System Web App"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SampleSystemWebAppIntegrationTest);

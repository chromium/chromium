// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_management/url_constants.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using PrintManagementAppIntegrationTest = ash::SystemWebAppIntegrationTest;

// Test that the Print Management App installs and launches correctly. Runs some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(PrintManagementAppIntegrationTest,
                       PrintManagementAppInLauncher) {
  const GURL url(ash::kChromeUIPrintManagementAppUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::PRINT_MANAGEMENT, url, "Print jobs"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PrintManagementAppIntegrationTest);

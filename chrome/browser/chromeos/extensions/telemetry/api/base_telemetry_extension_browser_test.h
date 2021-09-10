// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

#include <string>

#include "chrome/browser/extensions/extension_browsertest.h"

namespace chromeos {

class BaseTelemetryExtensionBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  BaseTelemetryExtensionBrowserTest();
  ~BaseTelemetryExtensionBrowserTest() override;

  BaseTelemetryExtensionBrowserTest(const BaseTelemetryExtensionBrowserTest&) =
      delete;
  BaseTelemetryExtensionBrowserTest& operator=(
      const BaseTelemetryExtensionBrowserTest&) = delete;

  // BrowserTestBase:
  void SetUpOnMainThread() override;

 protected:
  void CreateExtensionAndRunServiceWorker(
      const std::string& service_worker_content);
};

class BaseTelemetryExtensionApiAllowedBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  BaseTelemetryExtensionApiAllowedBrowserTest();
  ~BaseTelemetryExtensionApiAllowedBrowserTest() override;

  BaseTelemetryExtensionApiAllowedBrowserTest(
      const BaseTelemetryExtensionApiAllowedBrowserTest&) = delete;
  BaseTelemetryExtensionApiAllowedBrowserTest& operator=(
      const BaseTelemetryExtensionApiAllowedBrowserTest&) = delete;

  // BrowserTestBase:
  void SetUpOnMainThread() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

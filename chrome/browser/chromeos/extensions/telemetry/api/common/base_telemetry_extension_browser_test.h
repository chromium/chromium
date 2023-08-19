// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
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
  std::string extension_id() const;
  virtual std::string public_key() const;
  virtual std::string pwa_page_url() const;
  virtual std::string matches_origin() const;
  void CreateExtensionAndRunServiceWorker(
      const std::string& service_worker_content);
  void OpenAppUiAndMakeItSecure();
  virtual std::string GetManifestFile(const std::string& manifest_key,
                                      const std::string& matches_origin);

  std::unique_ptr<ApiGuardDelegate::Factory> api_guard_delegate_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

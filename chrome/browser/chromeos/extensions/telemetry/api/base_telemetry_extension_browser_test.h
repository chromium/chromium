// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/render_frame_host.h"

namespace chromeos {

struct ExtensionInfoTestParams {
  ExtensionInfoTestParams(const std::string& extension_id,
                          const std::string& public_key,
                          const std::string& pwa_page_url,
                          const std::string& matches_origin);
  ExtensionInfoTestParams(const ExtensionInfoTestParams& other);
  ~ExtensionInfoTestParams();

  const std::string extension_id;
  const std::string public_key;
  const std::string pwa_page_url;
  const std::string matches_origin;
};

class BaseTelemetryExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<ExtensionInfoTestParams> {
 public:
  static const std::vector<ExtensionInfoTestParams> kAllExtensionInfoTestParams;

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
  virtual std::string GetManifestFile(const std::string& public_key,
                                      const std::string& matches_origin);

  std::unique_ptr<HardwareInfoDelegate::Factory>
      hardware_info_delegate_factory_;

  bool should_open_pwa_ui_ = true;
  content::RenderFrameHost* pwa_page_rfh_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

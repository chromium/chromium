// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

#include <string>

#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

class BaseTelemetryExtensionBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  static const char kPwaPageUrlString[];

  BaseTelemetryExtensionBrowserTest();
  ~BaseTelemetryExtensionBrowserTest() override;

  BaseTelemetryExtensionBrowserTest(const BaseTelemetryExtensionBrowserTest&) =
      delete;
  BaseTelemetryExtensionBrowserTest& operator=(
      const BaseTelemetryExtensionBrowserTest&) = delete;

  // BrowserTestBase:
  void SetUpOnMainThread() override;

 protected:
  const extensions::Extension* LoadExtensionWithManifestAndServiceWorker(
      extensions::TestExtensionDir& test_dir,
      const std::string& manifest_content,
      const std::string& service_worker_content);
  const extensions::Extension* LoadExtensionWithServiceWorker(
      extensions::TestExtensionDir& test_dir,
      const std::string& service_worker_content);
  void CreateExtensionAndRunServiceWorker(
      const std::string& service_worker_content);

  std::unique_ptr<HardwareInfoDelegate::Factory>
      hardware_info_delegate_factory_;

  bool should_open_pwa_ui_ = true;
  content::RenderFrameHost* pwa_page_rfh_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

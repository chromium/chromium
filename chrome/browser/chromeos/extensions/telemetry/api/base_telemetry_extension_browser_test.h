// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/test_management_policy.h"

namespace ash {
class FakeChromeUserManager;
}

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

// The base test class for all TelemetryExtension browser tests. All tests are
// parameterized with the following parameters:
// * |is_user_affiliated| - a flag used to setup the testing environment for two
//                          scenarios: affiliated and normal user.
// * |extension_info_params| - parameters of the extension under test.
// Note: All tests must be defined using the IN_PROC_BROWSER_TEST_P macro and
// must use the INSTANTIATE_TEST_SUITE_P macro to instantiate the test suite.
class BaseTelemetryExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool, ExtensionInfoTestParams>> {
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
  void TearDownOnMainThread() override;

 protected:
  bool is_user_affiliated() const;
  ExtensionInfoTestParams extension_info_params() const;
  ash::FakeChromeUserManager* GetFakeUserManager() const;
  void CreateExtensionAndRunServiceWorker(
      const std::string& service_worker_content);
  virtual std::string GetManifestFile(const std::string& public_key,
                                      const std::string& matches_origin);

  std::unique_ptr<ApiGuardDelegate::Factory> api_guard_delegate_factory_;
  std::unique_ptr<HardwareInfoDelegate::Factory>
      hardware_info_delegate_factory_;
  bool should_open_pwa_ui_ = true;
  content::RenderFrameHost* pwa_page_rfh_ = nullptr;

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_BROWSER_TEST_H_

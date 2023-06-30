// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_ML_PROMOTION_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_INSTALLABLE_ML_PROMOTION_BROWSERTEST_BASE_H_

#include <string>

#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace segmentation_platform {
class MockSegmentationPlatformService;
}  // namespace segmentation_platform

namespace webapps {

class MLPromotionBrowserTestBase : public PlatformBrowserTest {
 public:
  MLPromotionBrowserTestBase();
  ~MLPromotionBrowserTestBase() override;

  // PlatformBrowserTest override. For desktop based tests, these 2 functions
  // should be in sync with the functions in WebAppControllerBrowsertest so as
  // to ensure that all dependencies are correctly handled.
  void SetUp() override;
  void SetUpOnMainThread() override;

  // Installs the app for the current web contents, skipping any dialogs.
  bool InstallAppForCurrentWebContents(bool install_locally);

  // Triggers a user-initiated install for the current web contents, showing any
  // install ux. If `accept_install` is true, then this will accept the install
  // & continue. If it is false, then it will cancel the install.
  // Returns if the installation went as expected.
  bool InstallAppFromUserInitiation(bool accept_install,
                                    std::string dialog_name);

  bool NavigateAndAwaitInstallabilityCheck(const GURL& url);
  segmentation_platform::MockSegmentationPlatformService* GetMockSegmentation(
      content::WebContents* custom_web_contents = nullptr);

  net::EmbeddedTestServer* https_server();
  content::WebContents* web_contents();
  Profile* profile();

 private:
#if !BUILDFLAG(IS_ANDROID)
  absl::optional<web_app::OsIntegrationManager::ScopedSuppressForTesting>
      os_hooks_suppress_;
#endif  // BUILDFLAG(IS_ANDROID)
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier cert_verifier_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_INSTALLABLE_ML_PROMOTION_BROWSERTEST_BASE_H_

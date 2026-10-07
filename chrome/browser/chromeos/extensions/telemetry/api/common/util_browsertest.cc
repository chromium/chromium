// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/webui/shimless_rma/backend/external_app_dialog.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kExtensionId[] = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
constexpr char kPwaPattern[] =
    "*://googlechromelabs.github.io/cros-sample-telemetry-extension/test-page/"
    "*";
constexpr char kPwaUrl[] =
    "https://googlechromelabs.github.io/cros-sample-telemetry-extension/"
    "test-page";
constexpr char kNotMatchedUrl[] = "https://example.com";

constexpr char kIwaExtensionId[] = "mconamggkmbalafmibfjlcmimnlbgmlb";
constexpr char kIwaPattern[] =
    "isolated-app://"
    "huhncggoe22ofjan6nylwijltmewmbevapiotudwgbyjbhrlphrqaaic/*";
constexpr char kIwaUrl[] =
    "isolated-app://"
    "huhncggoe22ofjan6nylwijltmewmbevapiotudwgbyjbhrlphrqaaic";

}  // namespace

class TelemetryExtensionUtilBrowserTest : public InProcessBrowserTest {
 public:
  TelemetryExtensionUtilBrowserTest() = default;
  ~TelemetryExtensionUtilBrowserTest() override = default;

  void TearDownOnMainThread() override {
    if (auto* contents =
            ash::shimless_rma::ExternalAppDialog::GetWebContents()) {
      content::WebContentsDestroyedWatcher watcher(contents);
      ash::shimless_rma::ExternalAppDialog::CloseForTesting();
      watcher.Wait();
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  Profile* profile() { return GetProfile(); }

  content::WebContents* AddTab(BrowserWindowInterface* target_browser,
                               const GURL& url) {
    return content::WebContents::FromRenderFrameHost(
        ui_test_utils::NavigateToURLWithDisposition(
            target_browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  content::WebContents* OpenAppUiUrlAndSetCertificateWithStatus(
      const GURL& url,
      net::CertStatus cert_status) {
    content::WebContents* web_contents = AddTab(browser(), url);
    SetCertificateWithStatus(web_contents, cert_status);
    return web_contents;
  }

  void SetCertificateWithStatus(content::WebContents* web_contents,
                                net::CertStatus cert_status) {
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& extension_id,
      const std::vector<std::string>& external_connectables) {
    base::ListValue matches;
    for (const auto& match : external_connectables) {
      matches.Append(match);
    }
    return extensions::ExtensionBuilder("Test ChromeOS System Extension")
        .SetManifestKey("chromeos_system_extension", base::DictValue())
        .SetManifestKey("externally_connectable",
                        base::DictValue().Set("matches", std::move(matches)))
        .SetID(extension_id)
        .SetLocation(extensions::mojom::ManifestLocation::kInternal)
        .Build();
  }
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest, AppUiNotOpen) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});

  EXPECT_FALSE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest, AppUiOpenAndSecure) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  content::WebContents* app_contents = OpenAppUiUrlAndSetCertificateWithStatus(
      GURL(kPwaUrl), /*cert_status=*/net::OK);

  EXPECT_TRUE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(app_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/false));
  EXPECT_EQ(app_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       AppUiOpenNotMatchingUrl) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kNotMatchedUrl),
                                          /*cert_status=*/net::OK);

  EXPECT_FALSE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       AppUiOpenButNotSecure) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL(kPwaUrl), /*cert_status=*/net::CERT_STATUS_INVALID);

  EXPECT_FALSE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       AppUiOpenInBackgroundTab) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  content::WebContents* app_contents = OpenAppUiUrlAndSetCertificateWithStatus(
      GURL(kPwaUrl), /*cert_status=*/net::OK);

  // Open a new foreground tab so `app_contents` is in the background.
  AddTab(browser(), GURL(kNotMatchedUrl));

  EXPECT_TRUE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(app_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       AppUiOpenInUnfocusedBrowser) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  content::WebContents* app_contents = OpenAppUiUrlAndSetCertificateWithStatus(
      GURL(kPwaUrl), /*cert_status=*/net::OK);

  BrowserWindowInterface* new_browser = CreateBrowser(profile());
  ui_test_utils::DeprecatedFakeActivateBrowser(new_browser);

  EXPECT_TRUE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(app_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         profile(), extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       AppUiOpenInDifferentProfile) {
  auto extension = CreateExtension(kExtensionId, {kPwaPattern});
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl),
                                          /*cert_status=*/net::OK);

  Profile* other_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(
      IsTelemetryExtensionAppUiOpenAndSecure(other_profile, extension.get()));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         other_profile, extension.get(),
                         /*focused_ui_required=*/false));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         other_profile, extension.get(),
                         /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest, IwaOpenInBrowserTab) {
  auto extension = CreateExtension(kIwaExtensionId, {kIwaPattern});
  content::WebContents* web_contents = AddTab(browser(), GURL("about:blank"));
  std::ignore = content::NavigateToURL(web_contents, GURL(kIwaUrl));

  EXPECT_TRUE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(web_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/false));
  EXPECT_EQ(web_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                              profile(), extension.get(),
                              /*focused_ui_required=*/true));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionUtilBrowserTest,
                       ShimlessRmaExternalAppDialog) {
  auto extension = CreateExtension(kIwaExtensionId, {kIwaPattern});

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::RMA);

  ash::shimless_rma::ExternalAppDialog::InitParams params;
  params.context = profile();
  params.app_name = "App Name";
  params.content_url = GURL("about:blank");
  ash::shimless_rma::ExternalAppDialog::Show(params);

  content::WebContents* dialog_contents =
      ash::shimless_rma::ExternalAppDialog::GetWebContents();
  ASSERT_TRUE(dialog_contents);
  std::ignore = content::NavigateToURL(dialog_contents, GURL(kIwaUrl));

  EXPECT_TRUE(
      IsTelemetryExtensionAppUiOpenAndSecure(profile(), extension.get()));
  EXPECT_EQ(dialog_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                                 profile(), extension.get(),
                                 /*focused_ui_required=*/false));
  EXPECT_EQ(dialog_contents, FindTelemetryExtensionOpenAndSecureAppUi(
                                 profile(), extension.get(),
                                 /*focused_ui_required=*/true));

  Profile* other_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(
      IsTelemetryExtensionAppUiOpenAndSecure(other_profile, extension.get()));
  EXPECT_EQ(nullptr, FindTelemetryExtensionOpenAndSecureAppUi(
                         other_profile, extension.get(),
                         /*focused_ui_required=*/false));
}

}  // namespace chromeos

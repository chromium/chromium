// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_BASE_TEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace ash {

using ::extensions::mojom::ManifestLocation;

// This app creates a window and declares usage of the identity API in its
// manifest, so we can test device robot token minting via the identity API.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/gcpjojfkologpegommokeppihdbcnahn
extern const char kTestEnterpriseKioskAppId[];

extern const char kTestEnterpriseAccountId[];

// This is a simple test chrome app that does not have `kiosk_enabled` flag in
// manifest. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/gbcgichpbeeimejckkpgnaighpndpped
constexpr char kTestNonKioskEnabledApp[] = "gbcgichpbeeimejckkpgnaighpndpped";

extern const test::UIPath kConfigNetwork;
extern const char kSizeChangedMessage[];

// Waits until `session` observes a new browser window was created, and returns
// whether this new window is closing.
bool DidSessionCloseNewWindow(KioskSystemSession* session);

// Opens accessibility settings browser and waits until it will be handled by
// `session`.
Browser* OpenA11ySettingsBrowser(KioskSystemSession* session);

// Base class for Chrome App Kiosk browser tests.
class KioskBaseTest : public OobeBaseTest {
 public:
  KioskBaseTest();

  KioskBaseTest(const KioskBaseTest&) = delete;
  KioskBaseTest& operator=(const KioskBaseTest&) = delete;

  ~KioskBaseTest() override;

 protected:
  using NetworkStatus = NetworkPortalDetectorMixin::NetworkStatus;

  // Waits for window width to change. Listens to a 'size_change' message sent
  // from DOM automation to `message_queue`.
  // The message is expected to be in JSON format:
  // {'name': <msg_name>, 'data': <extra_msg_data>}.
  // This will wait until a message with a different width is seen. It will
  // return the new width.
  static int WaitForWidthChange(content::DOMMessageQueue* message_queue,
                                int current_width);

  void SetUp() override;

  void TearDown() override;

  void SetUpOnMainThread() override;

  void TearDownOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  bool LaunchApp(const std::string& app_id);

  void ReloadKioskApps();

  void SetupTestAppUpdateCheck();

  void ReloadAutolaunchKioskApps();

  void PrepareAppLaunch();

  void StartAppLaunchFromLoginScreen(NetworkStatus network_status);
  void StartExistingAppLaunchFromLoginScreen(NetworkStatus network_status);

  const extensions::Extension* GetInstalledApp();

  const base::Version& GetInstalledAppVersion();

  ManifestLocation GetInstalledAppLocation();

  void WaitForAppLaunchWithOptions(bool check_launch_data,
                                   bool terminate_app,
                                   bool keep_app_open = false);

  void WaitForAppLaunchSuccess();

  void SimulateNetworkOnline();

  void SimulateNetworkOffline();

  void BlockAppLaunch(bool block);

  // If `crx_file` is empty string, sets `test_crx_file_` to `app_id` + ".crx".
  void SetTestApp(const std::string& app_id,
                  const std::string& version = "1.0.0",
                  const std::string& crx_file = "");

  KioskApp test_kiosk_app() const;
  const std::string& test_app_id() const { return test_app_id_; }
  const std::string& test_app_version() const { return test_app_version_; }
  const std::string& test_crx_file() const { return test_crx_file_; }
  FakeCWS* fake_cws() { return fake_cws_.get(); }

  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;

  const AccountId test_owner_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestOwnerEmail, "111");

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  // We need Fake gaia to avoid network errors that can be caused by
  // attempts to load real GAIA.
  FakeGaiaMixin fake_gaia_{&mixin_host_};

 private:
  // Email of owner account for test.
  constexpr static const char kTestOwnerEmail[] = "owner@example.com";

  std::string test_app_id_;
  std::string test_app_version_;
  std::string test_crx_file_;
  std::unique_ptr<FakeCWS> fake_cws_;

  base::AutoReset<bool> skip_splash_wait_override_ =
      KioskTestHelper::SkipSplashScreenWait();
  base::AutoReset<base::TimeDelta> network_wait_time_override_ =
      NetworkUiController::SetNetworkWaitTimeoutForTesting(
          base::Milliseconds(1));

  std::optional<base::AutoReset<bool>> block_app_launch_override_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_BASE_TEST_H_

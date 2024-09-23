// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/app_mode/web_kiosk_installer_lacros.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/browser_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::KioskSessionService;
using crosapi::mojom::SessionType;

namespace {

const char kWebAppUrl[] = "https://www.google.com/";

// Runs the provided callback when the web kiosk is initialized.
class KioskWebSessionInitializedWaiter
    : public KioskSessionServiceLacros::Observer {
 public:
  explicit KioskWebSessionInitializedWaiter(
      base::OnceClosure on_kiosk_web_session_initialized)
      : on_kiosk_web_session_initialized_(
            std::move(on_kiosk_web_session_initialized)) {
    kiosk_session_observation_.Observe(KioskSessionServiceLacros::Get());
  }

  ~KioskWebSessionInitializedWaiter() override = default;

  // KioskSessionServiceLacros::Observer:
  void KioskWebSessionInitialized() override {
    std::move(on_kiosk_web_session_initialized_).Run();
  }

 private:
  base::OnceCallback<void()> on_kiosk_web_session_initialized_;
  base::ScopedObservation<KioskSessionServiceLacros,
                          KioskSessionServiceLacros::Observer>
      kiosk_session_observation_{this};
};

Profile& GetProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

void SetBrowserInitParamsForWebKiosk() {
  BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->session_type = SessionType::kWebKioskSession;
  init_params->device_settings = crosapi::mojom::DeviceSettings::New();
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

void CreateKioskAppWindowAndWaitInitialized() {
  base::test::TestFuture<void> kiosk_initialized;
  KioskWebSessionInitializedWaiter waiter(kiosk_initialized.GetCallback());
  Browser::CreateParams params = web_app::CreateParamsForApp(
      kWebAppUrl,
      /*is_popup*/ true,
      /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(), &GetProfile(),
      /*user_gesture=*/true);
  web_app::CreateWebAppWindowMaybeWithHomeTab(kWebAppUrl, params);
  EXPECT_TRUE(kiosk_initialized.Wait());
  EXPECT_NE(
      KioskSessionServiceLacros::Get()->GetKioskBrowserSessionForTesting(),
      nullptr);
}

[[nodiscard]] bool WaitUntilBrowserClosed(Browser* browser) {
  TestBrowserClosedWaiter waiter(browser);
  return waiter.WaitUntilClosed();
}

bool DidSessionCloseNewWindow() {
  chromeos::KioskBrowserSession* session =
      KioskSessionServiceLacros::Get()->GetKioskBrowserSessionForTesting();
  base::test::TestFuture<bool> future;
  session->SetOnHandleBrowserCallbackForTesting(future.GetRepeatingCallback());
  return future.Take();
}

Browser* CreateBrowserWithWindowOfType(WindowOpenDisposition window_type) {
  Browser::CreateParams params = web_app::CreateParamsForApp(
      kWebAppUrl, window_type == WindowOpenDisposition::NEW_POPUP,
      /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(), &GetProfile(),
      /*user_gesture=*/true);
  Browser* browser =
      web_app::CreateWebAppWindowMaybeWithHomeTab(kWebAppUrl, params);
  browser->window()->Show();
  return browser;
}

}  // namespace

class WebKioskSessionServiceBrowserTest : public InProcessBrowserTest {
 protected:
  WebKioskSessionServiceBrowserTest() = default;
  ~WebKioskSessionServiceBrowserTest() override = default;

  void SetUp() override {
    // `SetBrowserInitParamsForWebKiosk` must be called before
    // `InProcessBrowserTest::SetUp` to configure `KioskSessionServiceLacros`
    // correctly.
    SetBrowserInitParamsForWebKiosk();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    attempt_user_exit_reset_ =
        KioskSessionServiceLacros::Get()->SetAttemptUserExitCallbackForTesting(
            base::DoNothing());

    installer_ = std::make_unique<WebKioskInstallerLacros>(GetProfile());
    InstallWebKiosk(kWebAppUrl);
  }

  void TearDownOnMainThread() override {
    attempt_user_exit_reset_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::optional<webapps::AppId> InstallWebKiosk(const std::string& url) {
    base::test::TestFuture<crosapi::mojom::WebKioskInstallState,
                           const std::optional<webapps::AppId>&>
        install_state;
    installer_->GetWebKioskInstallState(GURL(url), install_state.GetCallback());
    if (install_state.Get<0>() ==
        crosapi::mojom::WebKioskInstallState::kInstalled) {
      return install_state.Get<1>();
    }

    base::test::TestFuture<const std::optional<webapps::AppId>&> install_result;
    installer_->InstallWebKiosk(GURL(url), install_result.GetCallback());
    return install_result.Get();
  }

 private:
  std::unique_ptr<WebKioskInstallerLacros> installer_;
  std::unique_ptr<base::AutoReset<base::OnceClosure>> attempt_user_exit_reset_;
};

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       BrowserKioskSessionIsCreated) {
  EXPECT_EQ(
      KioskSessionServiceLacros::Get()->GetKioskBrowserSessionForTesting(),
      nullptr);

  CreateKioskAppWindowAndWaitInitialized();
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest, VerifyInstallUrl) {
  CreateKioskAppWindowAndWaitInitialized();

  EXPECT_EQ(KioskSessionServiceLacros::Get()->GetInstallURL(),
            GURL(kWebAppUrl));
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       ClosingAllWindowsTriggersAttemptUserExitCall) {
  // Closing all browser windows should trigger `AttemptUserExit`.
  base::test::TestFuture<void> did_attempt_user_exit;
  auto auto_reset =
      KioskSessionServiceLacros::Get()->SetAttemptUserExitCallbackForTesting(
          did_attempt_user_exit.GetCallback());

  CreateKioskAppWindowAndWaitInitialized();
  CloseAllBrowsers();

  EXPECT_TRUE(did_attempt_user_exit.Wait());
  // Verify that all windows have been closed.
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       KioskOriginShouldGetUnlimitedStorage) {
  CreateKioskAppWindowAndWaitInitialized();

  // Verify the origin of the install URL has been granted unlimited storage
  EXPECT_TRUE(
      GetProfile().GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
          GURL(kWebAppUrl)));
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       AnyNewBrowsersInKioskNotAllowedByDefault) {
  EXPECT_FALSE(
      GetProfile().GetPrefs()->GetBoolean(prefs::kNewWindowsInKioskAllowed));
  CreateKioskAppWindowAndWaitInitialized();

  Browser* popup_browser =
      CreateBrowserWithWindowOfType(WindowOpenDisposition::NEW_POPUP);
  EXPECT_TRUE(WaitUntilBrowserClosed(popup_browser));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  Browser* const new_browser =
      CreateBrowserWithWindowOfType(WindowOpenDisposition::NEW_WINDOW);
  EXPECT_TRUE(WaitUntilBrowserClosed(new_browser));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       NewPopupBrowserInKioskAllowedByPolicy) {
  GetProfile().GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  CreateKioskAppWindowAndWaitInitialized();

  CreateBrowserWithWindowOfType(WindowOpenDisposition::NEW_POPUP);

  EXPECT_FALSE(DidSessionCloseNewWindow());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
}

IN_PROC_BROWSER_TEST_F(WebKioskSessionServiceBrowserTest,
                       NewRegularBrowserInKioskNotAllowedEvenByPolicy) {
  GetProfile().GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  CreateKioskAppWindowAndWaitInitialized();

  // `kNewWindowsInKioskAllowed` policy allows to open only popup windows. All
  // other windows will be closed.
  Browser* const new_browser =
      CreateBrowserWithWindowOfType(WindowOpenDisposition::NEW_WINDOW);

  EXPECT_TRUE(WaitUntilBrowserClosed(new_browser));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

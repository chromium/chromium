// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"

#include <optional>
#include <string>
#include <string_view>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/functional/function_ref.h"
#include "base/notimplemented.h"
#include "base/scoped_multi_source_observation.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "url/gurl.h"

namespace ash::kiosk::test {

namespace {

const extensions::Extension* FindInExtensionRegistry(Profile& profile,
                                                     std::string_view app_id) {
  return extensions::ExtensionRegistry::Get(&profile)->GetInstalledExtension(
      std::string(app_id));
}

// Helper to wait for `KioskSystemSession` to be initialized. This happens late
// during Kiosk launch.
class SessionInitializedWaiter : public KioskAppManagerObserver {
 public:
  SessionInitializedWaiter() {
    observation_.AddObservation(KioskChromeAppManager::Get());
    observation_.AddObservation(KioskWebAppManager::Get());
    observation_.AddObservation(KioskIwaManager::Get());
  }
  SessionInitializedWaiter(const SessionInitializedWaiter&) = delete;
  SessionInitializedWaiter& operator=(const SessionInitializedWaiter&) = delete;
  ~SessionInitializedWaiter() override = default;

  bool Wait() { return future_.Wait(); }

 private:
  // KioskAppManagerObserver:
  void OnKioskSessionInitialized() override { future_.SetValue(); }

  base::test::TestFuture<void> future_;

  base::ScopedMultiSourceObservation<KioskAppManagerBase,
                                     KioskAppManagerObserver>
      observation_{this};
};

// Waits for the browser window to be hidden or destroyed.
class TestBrowserHiddenWaiter : public views::WidgetObserver {
 public:
  explicit TestBrowserHiddenWaiter(Browser* browser) {
    EXPECT_TRUE(browser->window()->IsVisible());
    widget_observation_.Observe(browser->GetBrowserView().GetWidget());
  }

  ~TestBrowserHiddenWaiter() override { widget_observation_.Reset(); }

  [[nodiscard]] bool WaitUntilHidden() { return future_.Wait(); }

 private:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    if (!visible) {
      future_.SetValue();
    }
  }
  void OnWidgetDestroying(views::Widget* widget) override {
    widget_observation_.Reset();
    future_.SetValue();
  }

  base::ScopedObservation<views::Widget, WidgetObserver> widget_observation_{
      this};
  base::test::TestFuture<void> future_;
};

content::WebContents* GetActiveWebContents(const Browser& browser) {
  return browser.tab_strip_model()->GetActiveWebContents();
}

void AddWebContentsToBrowser(Browser& browser, Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(&profile));

  browser.tab_strip_model()->AddWebContents(std::move(web_contents), -1,
                                            ui::PAGE_TRANSITION_FIRST,
                                            AddTabTypes::ADD_ACTIVE);
}

void TriggerNavigationToUrl(content::WebContents* web_contents,
                            const GURL& url) {
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

}  // namespace

KioskApp AutoLaunchKioskApp() {
  auto app = KioskController::Get().GetAutoLaunchApp();
  CHECK(app.has_value());
  return app.value();
}

KioskApp TheKioskApp() {
  auto apps = KioskController::Get().GetApps();
  CHECK_EQ(apps.size(), 1ul);
  return apps[0];
}

KioskApp TheKioskChromeApp() {
  auto app = TheKioskApp();
  CHECK_EQ(app.id().type, KioskAppType::kChromeApp);
  return app;
}

KioskApp TheKioskWebApp() {
  auto app = TheKioskApp();
  CHECK_EQ(app.id().type, KioskAppType::kWebApp);
  return app;
}

std::optional<KioskApp> GetAppByAccountId(std::string_view account_id) {
  // We don't know which app type `account_id` refers to at this point. Create a
  // device local account ID for each app type and see which one exists.
  auto chrome_app_account_id = CreateDeviceLocalAccountId(
      account_id, policy::DeviceLocalAccountType::kKioskApp);
  auto web_app_account_id = CreateDeviceLocalAccountId(
      account_id, policy::DeviceLocalAccountType::kWebKioskApp);
  auto iwa_account_id = CreateDeviceLocalAccountId(
      account_id, policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
  for (const auto& app : KioskController::Get().GetApps()) {
    switch (app.id().type) {
      case KioskAppType::kChromeApp:
        if (app.id().account_id == chrome_app_account_id) {
          return app;
        }
        break;
      case KioskAppType::kWebApp:
        if (app.id().account_id == web_app_account_id) {
          return app;
        }
        break;
      case KioskAppType::kIsolatedWebApp:
        if (app.id().account_id == iwa_account_id) {
          return app;
        }
        break;
      case KioskAppType::kArcvmApp:
        NOTIMPLEMENTED();
    }
  }
  return std::nullopt;
}

bool WaitKioskLaunched() {
  if (KioskController::Get().GetKioskSystemSession() != nullptr) {
    // Kiosk session already initialized, nothing to wait for.
    return true;
  }
  return SessionInitializedWaiter().Wait();
}
bool LaunchAppManually(const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp:
      return LoginScreenTestApi::LaunchApp(app.id().app_id.value());
    case KioskAppType::kWebApp:
    case KioskAppType::kIsolatedWebApp:
    case KioskAppType::kArcvmApp:
      return LoginScreenTestApi::LaunchApp(app.id().account_id);
  }
}

bool LaunchAppManually(std::string_view account_id) {
  auto app_maybe = GetAppByAccountId(account_id);
  return app_maybe.has_value() ? LaunchAppManually(app_maybe.value()) : false;
}

std::optional<base::AutoReset<bool>> BlockKioskLaunch() {
  return {KioskTestHelper::BlockAppLaunch()};
}

bool IsChromeAppInstalled(Profile& profile, const KioskApp& app) {
  CHECK_EQ(app.id().type, KioskAppType::kChromeApp);
  return IsChromeAppInstalled(profile, app.id().app_id.value());
}

bool IsChromeAppInstalled(Profile& profile, std::string_view app_id) {
  return FindInExtensionRegistry(profile, app_id) != nullptr;
}

bool IsWebAppInstalled(Profile& profile, const KioskApp& app) {
  CHECK_EQ(app.id().type, KioskAppType::kWebApp);
  return IsWebAppInstalled(profile, app.url().value());
}

bool IsWebAppInstalled(Profile& profile, const GURL& install_url) {
  auto [state, __] = chromeos::GetKioskWebAppInstallState(profile, install_url);
  return chromeos::WebKioskInstallState::kInstalled == state;
}

bool IsAppInstalled(Profile& profile, const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp:
      return IsChromeAppInstalled(profile, app.id().app_id.value());
    case KioskAppType::kWebApp:
      return IsWebAppInstalled(profile, app.url().value());
    case KioskAppType::kIsolatedWebApp:
    case KioskAppType::kArcvmApp:
      // TODO(crbug.com/379633748): Support IWA in KioskMixin.
      NOTIMPLEMENTED();
      return false;
  }
}

std::string InstalledChromeAppVersion(Profile& profile, const KioskApp& app) {
  CHECK_EQ(app.id().type, KioskAppType::kChromeApp);
  return InstalledChromeAppVersion(profile, app.id().app_id.value());
}

std::string InstalledChromeAppVersion(Profile& profile,
                                      std::string_view app_id) {
  auto& chrome_app = CHECK_DEREF(FindInExtensionRegistry(profile, app_id));
  return chrome_app.version().GetString();
}

std::string CachedChromeAppVersion(const KioskApp& app) {
  return CachedChromeAppVersion(app.id().app_id.value());
}

std::string CachedChromeAppVersion(std::string_view app_id) {
  auto& manager = CHECK_DEREF(KioskChromeAppManager::Get());
  auto [_, cached_crx_version] = manager.GetCachedCrx(app_id).value();
  return cached_crx_version;
}

Profile& CurrentProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

void WaitSplashScreen() {
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
}

void WaitNetworkScreen() {
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
}

bool PressNetworkAccelerator() {
  return LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

bool PressBailoutAccelerator() {
  return LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchBailout);
}

Browser* OpenA11ySettings(const user_manager::User& user) {
  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  auto& settings_manager = CHECK_DEREF(ash::SettingsAppManager::Get());

  settings_manager.Open(
      user,
      {.sub_page = chromeos::settings::mojom::kManageAccessibilitySubpagePath});

  EXPECT_FALSE(DidKioskCloseNewWindow());

  Browser& settings_browser =
      CHECK_DEREF(session.GetSettingsBrowserForTesting());
  return &settings_browser;
}

bool DidKioskCloseNewWindow() {
  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  base::test::TestFuture<bool> new_window_closed;
  session.SetOnHandleBrowserCallbackForTesting(
      new_window_closed.GetRepeatingCallback());
  return new_window_closed.Take();
}

bool DidKioskHideNewWindow(Browser* browser) {
  return TestBrowserHiddenWaiter(browser).WaitUntilHidden();
}

void CloseAppWindow(const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp: {
      auto& registry =
          CHECK_DEREF(extensions::AppWindowRegistry::Get(&CurrentProfile()));
      auto& chrome_app_window = CHECK_DEREF(
          apps::AppWindowWaiter(&registry, app.id().app_id.value()).Wait());
      chrome_app_window.GetBaseWindow()->Close();
      break;
    }
    case KioskAppType::kWebApp:
    case KioskAppType::kIsolatedWebApp: {
      EXPECT_GE(chrome::GetTotalBrowserCount(), 1u);
      BrowserWindowInterface* web_app_browser = nullptr;
      // TODO(crbug.com/444072535): Picking a Browser from the global browser
      // list is flaky and very test dependent. This should be updated to
      // instead close the Browser instance specifically associated with `app`.
      ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
          [&web_app_browser](BrowserWindowInterface* browser) {
            if ((browser->GetType() ==
                 BrowserWindowInterface::Type::TYPE_APP) ||
                (browser->GetType() ==
                 BrowserWindowInterface::Type::TYPE_APP_POPUP)) {
              web_app_browser = browser;
              return false;
            }
            return true;
          });
      CHECK(web_app_browser);
      web_app_browser->GetWindow()->Close();
      break;
    }
    case KioskAppType::kArcvmApp:
      NOTIMPLEMENTED();
      break;
  }
}

void CachePolicy(const std::string& account_id,
                 base::FunctionRef<void(policy::UserPolicyBuilder&)> setup) {
  policy::UserPolicyBuilder builder;
  builder.policy_data().set_policy_type(
      policy::dm_protocol::kChromePublicAccountPolicyType);
  builder.policy_data().set_username(account_id);
  builder.SetDefaultSigningKey();

  setup(builder);
  builder.Build();

  const std::string policy_blob = builder.GetBlob();
  auto& manager = CHECK_DEREF(FakeSessionManagerClient::Get());
  manager.set_device_local_account_policy(account_id, policy_blob);
  manager.device_local_account_policy(account_id);
}

AccountId CreateDeviceLocalAccountId(std::string_view account_id,
                                     policy::DeviceLocalAccountType type) {
  return AccountId(AccountId::FromUserEmail(
      policy::GenerateDeviceLocalAccountUserId(account_id, type)));
}

Browser& CreateRegularBrowser(Profile& profile, const GURL& url) {
  Browser::CreateParams params(&profile, /*user_gesture=*/true);
  Browser& browser = CHECK_DEREF(Browser::Create(params));
  browser.window()->Show();

  AddWebContentsToBrowser(browser, profile);
  TriggerNavigationToUrl(GetActiveWebContents(browser), url);

  return browser;
}

Browser& CreatePopupBrowser(Profile& profile,
                            const std::string& app_name,
                            const GURL& url) {
  Browser::CreateParams params = Browser::CreateParams::CreateForAppPopup(
      app_name,
      /*trusted_source=*/true,
      /*window_bounds=*/gfx::Rect(), &profile,
      /*user_gesture=*/true);
  Browser& browser = CHECK_DEREF(Browser::Create(params));
  browser.window()->Show();

  AddWebContentsToBrowser(browser, profile);
  TriggerNavigationToUrl(GetActiveWebContents(browser), url);

  return browser;
}

}  // namespace ash::kiosk::test

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/test/kiosk_session_initialized_waiter.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

using enterprise_management::DeviceLocalAccountInfoProto;
using enterprise_management::DeviceLocalAccountsProto;

constexpr std::string_view kDefaultWebAppOrigin = "https://kioskmixinapp.com";

constexpr base::FilePath::StringPieceType kDefaultWebAppPath =
    FILE_PATH_LITERAL("chrome/test/data");

void AppendSwitchesToDisplayLoginScreen(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kLoginManager);
  command_line->AppendSwitch(switches::kForceLoginManagerInTests);
}

// The account ID as configured in policies is different from the `AccountId`
// that identify users in Chrome. This function converts the policy `account_id`
// of a Kiosk app of the given `type` to a Chrome `AccountId`.
AccountId CreateDeviceLocalAccountId(std::string_view account_id,
                                     policy::DeviceLocalAccountType type) {
  return AccountId(AccountId::FromUserEmail(
      policy::GenerateDeviceLocalAccountUserId(account_id, type)));
}

std::string_view GetAccountId(const KioskMixin::Option& option) {
  return std::visit(
      base::Overloaded{
          [](const KioskMixin::DefaultServerWebAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::WebAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::CwsChromeAppOption& option) {
            return std::string_view(option.account_id);
          },
      },
      option);
}

// Returns the Web app URL of the given `option`, or the empty `GURL` if it's
// not a Web app option.
GURL GetWebAppUrl(const KioskMixin::Option& option) {
  return std::visit(
      base::Overloaded{
          [](const KioskMixin::DefaultServerWebAppOption& option) {
            return GURL(kDefaultWebAppOrigin).Resolve(option.url_path);
          },
          [](const KioskMixin::WebAppOption& option) { return option.url; },
          [](const KioskMixin::CwsChromeAppOption& option) { return GURL(); },
      },
      option);
}

// Returns the Chrome app ID of the given `option`, or the empty string if it's
// not a Chrome app option.
std::string_view GetChromeAppId(const KioskMixin::Option& option) {
  return std::visit(
      base::Overloaded{
          [](const KioskMixin::DefaultServerWebAppOption& option) {
            return std::string_view();
          },
          [](const KioskMixin::WebAppOption& option) {
            return std::string_view();
          },
          [](const KioskMixin::CwsChromeAppOption& option) {
            return std::string_view(option.app_id);
          },
      },
      option);
}

// Runs multiple checks on `config` to avoid common errors.
void CheckIsValid(const KioskMixin::Config& config) {
  // There must be at least one Kiosk app.
  CHECK_NE(0ul, config.options.size());

  // No two apps can have the same account ID.
  auto configured_accounts = base::MakeFlatSet<std::string_view>(
      config.options, std::less(), GetAccountId);
  CHECK_EQ(configured_accounts.size(), config.options.size());

  // If there is an auto launch app, there must also be a config option for it.
  if (config.auto_launch_account_id.has_value()) {
    CHECK_NE(0l,
             base::ranges::count(config.options,
                                 config.auto_launch_account_id.value().value(),
                                 GetAccountId));
  }

  // No two Web apps can have the same URL.
  for (auto it = config.options.begin(); it != config.options.end(); it++) {
    if (GURL url = GetWebAppUrl(*it); url.is_valid()) {
      CHECK_EQ(1, base::ranges::count(config.options, url, GetWebAppUrl));
    }
  }

  // No two Chrome apps can have the same app ID.
  for (auto it = config.options.begin(); it != config.options.end(); it++) {
    if (std::string_view app_id = GetChromeAppId(*it); !app_id.empty()) {
      CHECK_EQ(1, base::ranges::count(config.options, app_id, GetChromeAppId));
    }
  }
}

// Configures a Kiosk Chrome App in device policies with the given `app_id`
// and `account_id`.
void ConfigureCwsChromeApp(ScopedDevicePolicyUpdate& update,
                           std::string_view app_id,
                           std::string_view account_id) {
  DeviceLocalAccountInfoProto* account =
      update.policy_payload()->mutable_device_local_accounts()->add_account();

  account->set_account_id(std::string(account_id));
  account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
  account->mutable_kiosk_app()->set_app_id(std::string(app_id));
}

// Configures a Kiosk web app in device policies with the given `url` and
// `account_id`.
void ConfigureWebApp(ScopedDevicePolicyUpdate& update,
                     const GURL& url,
                     std::string_view account_id) {
  DeviceLocalAccountInfoProto* account =
      update.policy_payload()->mutable_device_local_accounts()->add_account();

  account->set_account_id(std::string(account_id));
  account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_WEB_KIOSK_APP);
  account->mutable_web_kiosk_app()->set_url(url.spec());
}

// Configures the Kiosk account given by `account_id` as the auto launch
// account.
void ConfigureAutoLaunchAccountId(ScopedDevicePolicyUpdate& update,
                                  std::string_view account_id) {
  update.policy_payload()->mutable_device_local_accounts()->set_auto_login_id(
      std::string(account_id));
}

}  // namespace

KioskMixin::KioskMixin(InProcessBrowserTestMixinHost* host,
                       std::optional<Config> cached_configuration)
    : InProcessBrowserTestMixin(host),
      cached_configuration_(std::move(cached_configuration)),
      skip_splash_screen_override_(KioskTestHelper::SkipSplashScreenWait()),
      network_wait_override_(
          NetworkUiController::SetNetworkWaitTimeoutForTesting(
              base::TimeDelta())),
      web_server_(host, GURL(kDefaultWebAppOrigin), kDefaultWebAppPath),
      fake_cws_mixin_(host, FakeCwsMixin::kPublic),
      device_state_(
          host,
          ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED) {}

KioskMixin::KioskMixin(InProcessBrowserTestMixinHost* host,
                       Config cached_configuration)
    : KioskMixin(host, std::make_optional(std::move(cached_configuration))) {}

KioskMixin::KioskMixin(InProcessBrowserTestMixinHost* host)
    : KioskMixin(host, /*cached_configuration=*/{}) {}

KioskMixin::~KioskMixin() = default;

void KioskMixin::SetUpCommandLine(base::CommandLine* command_line) {
  AppendSwitchesToDisplayLoginScreen(command_line);
}

void KioskMixin::SetUpInProcessBrowserTestFixture() {
  if (cached_configuration_.has_value()) {
    Configure(*device_state_.RequestDevicePolicyUpdate().get(),
              cached_configuration_.value());
  }
}

void KioskMixin::Configure(ScopedDevicePolicyUpdate& scoped_update,
                           const Config& config) {
  CheckIsValid(config);
  for (const auto& option : config.options) {
    std::visit(
        base::Overloaded{
            [this, &scoped_update](const DefaultServerWebAppOption& option) {
              ConfigureWebApp(scoped_update,
                              web_server_.GetUrl(option.url_path),
                              option.account_id);
            },
            [&scoped_update](const WebAppOption& option) {
              ConfigureWebApp(scoped_update, option.url, option.account_id);
            },
            [this, &scoped_update](const CwsChromeAppOption& option) {
              fake_cws().SetUpdateCrx(option.app_id, option.crx_filename,
                                      option.crx_version);
              ConfigureCwsChromeApp(scoped_update, option.app_id,
                                    option.account_id);
            },
        },
        option);
  }
  if (config.auto_launch_account_id.has_value()) {
    ConfigureAutoLaunchAccountId(scoped_update,
                                 config.auto_launch_account_id->value());
  }
}

bool KioskMixin::LaunchManually(const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp:
      return LoginScreenTestApi::LaunchApp(app.id().app_id.value());
    case KioskAppType::kWebApp:
      return LoginScreenTestApi::LaunchApp(app.id().account_id);
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/379633748): Support IWA in KioskMixin.
      return false;
  }
}

bool KioskMixin::LaunchManually(std::string_view account_id) {
  auto app_maybe = GetAppByAccountId(account_id);
  return app_maybe.has_value() ? LaunchManually(app_maybe.value()) : false;
}

bool KioskMixin::WaitSessionLaunched() {
  return KioskSessionInitializedWaiter().Wait();
}

// static
std::optional<KioskApp> KioskMixin::GetAppByAccountId(
    std::string_view account_id) {
  // We don't know which app type `account_id` refers to at this point. Create a
  // device local account ID for each app type and see which one exists.
  auto chrome_app_account_id = CreateDeviceLocalAccountId(
      account_id, policy::DeviceLocalAccountType::kKioskApp);
  auto web_app_account_id = CreateDeviceLocalAccountId(
      account_id, policy::DeviceLocalAccountType::kWebKioskApp);
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
        // TODO(crbug.com/379633748): Support IWA in KioskMixin.
        break;
    }
  }
  return std::nullopt;
}

GURL KioskMixin::GetDefaultServerUrl(std::string_view url_suffix) const {
  return web_server_.GetUrl(url_suffix);
}

// static
std::vector<KioskMixin::Config> KioskMixin::ConfigsToAutoLaunchEachAppType() {
  return {
      Config{/*name=*/"WebApp",
             AutoLaunchAccount{SimpleWebAppOption().account_id},
             {SimpleWebAppOption()}},
      Config{/*name=*/"ChromeApp",
             AutoLaunchAccount{SimpleChromeAppOption().account_id},
             {SimpleChromeAppOption()}},
  };
}

// static
KioskMixin::DefaultServerWebAppOption KioskMixin::SimpleWebAppOption() {
  // Serves //chrome/test/data/title3.html.
  return DefaultServerWebAppOption{/*account_id=*/"simple-web-app@localhost",
                                   /*url_path=*/"/title3.html"};
}

// static
KioskMixin::CwsChromeAppOption KioskMixin::SimpleChromeAppOption() {
  // Configures the Chrome app in:
  //   //chrome/test/data/chromeos/app_mode/apps_and_extensions/kiosk_test_app.
  static constexpr char kChromeAppId[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";

  return CwsChromeAppOption{
      /*account_id=*/"simple-chrome-app@localhost",
      /*app_id=*/kChromeAppId,
      /*crx_filename=*/base::StrCat({kChromeAppId, ".crx"}),
      /*crx_version=*/"1.0.0"};
}

// static
std::string KioskMixin::ConfigName(const testing::TestParamInfo<Config>& info) {
  return info.param.name.value_or(base::StringPrintf("%zu", info.index));
}

KioskMixin::DefaultServerWebAppOption::DefaultServerWebAppOption(
    std::string_view account_id,
    std::string_view url_path)
    : account_id(std::string(account_id)), url_path(std::string(url_path)) {}

KioskMixin::DefaultServerWebAppOption::DefaultServerWebAppOption(
    const KioskMixin::DefaultServerWebAppOption&) = default;
KioskMixin::DefaultServerWebAppOption::DefaultServerWebAppOption(
    KioskMixin::DefaultServerWebAppOption&&) = default;
KioskMixin::DefaultServerWebAppOption&
KioskMixin::DefaultServerWebAppOption::operator=(
    const KioskMixin::DefaultServerWebAppOption&) = default;
KioskMixin::DefaultServerWebAppOption&
KioskMixin::DefaultServerWebAppOption::operator=(
    KioskMixin::DefaultServerWebAppOption&&) = default;
KioskMixin::DefaultServerWebAppOption::~DefaultServerWebAppOption() = default;

KioskMixin::WebAppOption::WebAppOption(std::string_view account_id, GURL url)
    : account_id(std::string(account_id)), url(std::move(url)) {}

KioskMixin::WebAppOption::WebAppOption(const KioskMixin::WebAppOption&) =
    default;
KioskMixin::WebAppOption::WebAppOption(KioskMixin::WebAppOption&&) = default;
KioskMixin::WebAppOption& KioskMixin::WebAppOption::operator=(
    const KioskMixin::WebAppOption&) = default;
KioskMixin::WebAppOption& KioskMixin::WebAppOption::operator=(
    KioskMixin::WebAppOption&&) = default;
KioskMixin::WebAppOption::~WebAppOption() = default;

KioskMixin::CwsChromeAppOption::CwsChromeAppOption(
    std::string_view account_id,
    std::string_view app_id,
    std::string_view crx_filename,
    std::string_view crx_version)
    : account_id(std::string(account_id)),
      app_id(std::string(app_id)),
      crx_filename(std::string(crx_filename)),
      crx_version(std::string(crx_version)) {}

KioskMixin::CwsChromeAppOption::CwsChromeAppOption(
    const KioskMixin::CwsChromeAppOption&) = default;
KioskMixin::CwsChromeAppOption::CwsChromeAppOption(
    KioskMixin::CwsChromeAppOption&&) = default;
KioskMixin::CwsChromeAppOption& KioskMixin::CwsChromeAppOption::operator=(
    const KioskMixin::CwsChromeAppOption&) = default;
KioskMixin::CwsChromeAppOption& KioskMixin::CwsChromeAppOption::operator=(
    KioskMixin::CwsChromeAppOption&&) = default;
KioskMixin::CwsChromeAppOption::~CwsChromeAppOption() = default;

KioskMixin::Config::Config(
    std::optional<std::string> name,
    std::optional<AutoLaunchAccount> auto_launch_account_id,
    std::vector<Option> options)
    : name(std::move(name)),
      auto_launch_account_id(std::move(auto_launch_account_id)),
      options(std::move(options)) {}

KioskMixin::Config::Config(const KioskMixin::Config&) = default;
KioskMixin::Config::Config(KioskMixin::Config&&) = default;
KioskMixin::Config& KioskMixin::Config::Config::operator=(
    const KioskMixin::Config&) = default;
KioskMixin::Config& KioskMixin::Config::Config::operator=(
    KioskMixin::Config&&) = default;
KioskMixin::Config::~Config() = default;

}  // namespace ash

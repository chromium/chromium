// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_MIXIN_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_MIXIN_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

// This mixin helps browser tests set up the browser for Kiosk mode. It enrolls
// the device and can set policies in Kiosk device local accounts based on a
// `Config`.
//
// Prefer this mixin when writing Kiosk browser tests because its set up is the
// most realistic, and allows tests for web apps and Chrome apps in the same
// fixture.
class KioskMixin : public InProcessBrowserTestMixin {
 public:
  // Option for a web app configured to use `web_server_`. `url_path` should
  // refer to some content under //chrome/test/data.
  struct DefaultServerWebAppOption {
    DefaultServerWebAppOption(std::string_view account_id,
                              std::string_view url_path);

    DefaultServerWebAppOption(const DefaultServerWebAppOption&);
    DefaultServerWebAppOption(DefaultServerWebAppOption&&);
    DefaultServerWebAppOption& operator=(const DefaultServerWebAppOption&);
    DefaultServerWebAppOption& operator=(DefaultServerWebAppOption&&);
    ~DefaultServerWebAppOption();

    std::string account_id;
    std::string url_path;
  };

  // Option for a generic web app served at the given `url`. When using this
  // option make sure to also create the actual server before launching the app.
  struct WebAppOption {
    WebAppOption(std::string_view account_id, GURL url);

    WebAppOption(const WebAppOption&);
    WebAppOption(WebAppOption&&);
    WebAppOption& operator=(const WebAppOption&);
    WebAppOption& operator=(WebAppOption&&);
    ~WebAppOption();

    std::string account_id;
    GURL url;
  };

  // Option for a Chrome app hosted in the `FakeCWS`. Refer to the files under
  // //chrome/test/data/chromeos/app_mode to see what apps are configured.
  struct CwsChromeAppOption {
    CwsChromeAppOption(std::string_view account_id,
                       std::string_view app_id,
                       std::string_view crx_filename,
                       std::string_view crx_version);

    CwsChromeAppOption(const CwsChromeAppOption&);
    CwsChromeAppOption(CwsChromeAppOption&&);
    CwsChromeAppOption& operator=(const CwsChromeAppOption&);
    CwsChromeAppOption& operator=(CwsChromeAppOption&&);
    ~CwsChromeAppOption();

    std::string account_id;
    std::string app_id;
    std::string crx_filename;
    std::string crx_version;
  };

  // Option for a Chrome app hosted outside CWS. Tests should make sure the app
  // is actually hosted in `update_url`, for example using `FakeCwsMixin`.
  struct SelfHostedChromeAppOption {
    SelfHostedChromeAppOption(std::string_view account_id,
                              std::string_view app_id,
                              const GURL& update_url);

    SelfHostedChromeAppOption(const SelfHostedChromeAppOption&);
    SelfHostedChromeAppOption(SelfHostedChromeAppOption&&);
    SelfHostedChromeAppOption& operator=(const SelfHostedChromeAppOption&);
    SelfHostedChromeAppOption& operator=(SelfHostedChromeAppOption&&);
    ~SelfHostedChromeAppOption();

    std::string account_id;
    std::string app_id;
    GURL update_url;
  };

  // Option for an Isolated Web App served at the given `update_manifest_url`.
  // When using this option make sure to also create the actual server before
  // launching the app.
  struct IsolatedWebAppOption {
    IsolatedWebAppOption(std::string_view account_id,
                         const web_package::SignedWebBundleId& web_bundle_id,
                         GURL update_manifest_url,
                         std::string update_channel = "",
                         std::string pinned_version = "",
                         bool allow_downgrades = false,
                         bool skip_iwa_allowlist_checks = true);

    IsolatedWebAppOption(const IsolatedWebAppOption&);
    IsolatedWebAppOption(IsolatedWebAppOption&&);
    IsolatedWebAppOption& operator=(const IsolatedWebAppOption&);
    IsolatedWebAppOption& operator=(IsolatedWebAppOption&&);
    ~IsolatedWebAppOption();

    std::string account_id;
    web_package::SignedWebBundleId web_bundle_id;
    GURL update_manifest_url;
    std::string update_channel;
    std::string pinned_version;
    bool allow_downgrades;
    // Only IWA allowlisted through Key Distribution component can be installed.
    // Allowlisting through component update may interfere with other component
    // updates in unexpected way, so skipping is preferred for most tests.
    bool skip_iwa_allowlist_checks;
  };

  // The account ID of the app that Kiosk should auto launch, as configured in
  // policies.
  using AutoLaunchAccount =
      base::StrongAlias<class AutoLaunchAccountTag, std::string>;

  // The possible options that can be used to configure `KioskMixin`.
  using Option = std::variant<DefaultServerWebAppOption,
                              WebAppOption,
                              CwsChromeAppOption,
                              SelfHostedChromeAppOption,
                              IsolatedWebAppOption>;

  // Encapsulates the data used to configure Kiosk.
  //
  // This is designed to be used as the parameter type in parameterized tests,
  // for example with `ConfigsToAutoLaunchEachAppType()`.
  struct Config {
    Config(std::optional<std::string> name,
           std::optional<AutoLaunchAccount> auto_launch_account_id,
           std::vector<Option> options);
    Config(const Config&);
    Config(Config&&);
    Config& operator=(const Config&);
    Config& operator=(Config&&);
    ~Config();

    // The name of this `Config`. Used by `ConfigName` to display a better name
    // in parameterized tests.
    std::optional<std::string> name;
    // The `account_id` to auto launch. The corresponding app must be configured
    // in `options`.
    std::optional<AutoLaunchAccount> auto_launch_account_id;
    // The list of `Option`s to configure Kiosk.
    std::vector<Option> options;
  };

  // Helper alias to create `ScopedUserPolicyUpdate` instances for `Configure`.
  using UserPolicyUpdateCallback =
      base::RepeatingCallback<std::unique_ptr<ScopedUserPolicyUpdate>(
          std::string_view)>;

  // Returns the gtest parameter name for this `Config`.
  static std::string ConfigName(const testing::TestParamInfo<Config>& info);

  // Returns a list of `Config`s to auto launch each application type supported
  // in Kiosk.
  //
  // This is useful to instantiate parameterized tests with
  // `INSTANTIATE_TEST_SUITE_P` for tests that verify Kiosk features independent
  // of application type.
  static std::vector<Config> ConfigsToAutoLaunchEachAppType();

  // `Option` to configure a simple web app.
  static DefaultServerWebAppOption SimpleWebAppOption();

  // `Option` to configure a simple Chrome app.
  static CwsChromeAppOption SimpleChromeAppOption();

  // Uses `cached_configuration` to set up Kiosk policies. The configuration is
  // set in the beginning of the test, simulating policies being pre-cached in
  // the device.
  //
  // See the constructor below to set Kiosk policies during the test.
  KioskMixin(InProcessBrowserTestMixinHost* host, Config cached_configuration);

  // Creates an instance without any pre-cached Kiosk configuration. Tests that
  // use this constructor are expected to call `Configure` to set up Kiosk
  // policies during the test.
  //
  // See the constructor above to pre-cache Kiosk policies in the beginning of
  // the test.
  explicit KioskMixin(InProcessBrowserTestMixinHost* host);

  KioskMixin(const KioskMixin&) = delete;
  KioskMixin& operator=(const KioskMixin&) = delete;

  ~KioskMixin() override;

  // Sets up Kiosk device policies corresponding to `config` in the given
  // `device_policy_update`, and sets device local account policies
  // for each account in `config` via the updates returned by
  // `user_policy_update_callback`.
  //
  // This can be used to simulate a policy change mid test, for example when
  // combined with a `policy::DevicePolicyCrosTestHelper`.
  void Configure(ScopedDevicePolicyUpdate& device_policy_update,
                 UserPolicyUpdateCallback user_policy_update_callback,
                 const Config& config);
  // Similar to above but defaults to a `user_policy_update_callback` that sets
  // policies using `DeviceStateMixin`.
  void Configure(ScopedDevicePolicyUpdate& device_policy_update,
                 const Config& config);

  // Returns a URL to the default web server with `url_suffix` appended to it.
  //
  // `url_suffix` must start with "/".
  GURL GetDefaultServerUrl(std::string_view url_suffix) const;

  FakeCWS& fake_cws() { return fake_cws_mixin_.fake_cws(); }

  DeviceStateMixin& device_state_mixin() { return device_state_; }

 private:
  KioskMixin(InProcessBrowserTestMixinHost* host,
             std::optional<Config> cached_configuration);

  // `InProcessBrowserTestMixin`:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  // The configuration used to simulate pre-cached policy state before the test
  // starts.
  std::optional<Config> cached_configuration_;

  // Used to skip the splash screen timer during launching. Improves test speed.
  base::AutoReset<bool> skip_splash_screen_override_;

  // Used to display the network dialog sooner when there is no network.
  base::AutoReset<base::TimeDelta> network_wait_override_;

  // Holds the embedded test server for the default web app.
  FakeOriginTestServerMixin web_server_;

  // Holds the `FakeCWS` to host test Chrome apps.
  FakeCwsMixin fake_cws_mixin_;

  // Used to enroll the device and simulate pre-cached policy state.
  DeviceStateMixin device_state_;

  // Used to enable Chrome apps in Kiosk in tests that need it.
  base::test::ScopedFeatureList scoped_features_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_MIXIN_H_

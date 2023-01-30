// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_service_impl.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace {

constexpr char kDefaultAppInstallUrl[] = "https://example.com/install";
constexpr char kTrustedUrl[] = "https://example.com/sample";
constexpr char kKioskAppInstallUrl[] = "https://kiosk.com/install";
constexpr char kUserEmail[] = "user-email@example.com";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kUntrustedUrl[] = "https://non-example.com/sample";
constexpr char kKioskAppUrl[] = "https://kiosk.com/sample";
constexpr char kInvalidKioskAppUrl[] = "https://invalid-kiosk.com/sample";
#endif

void VerifyErrorMessageResult(blink::mojom::DeviceAttributeResultPtr result) {
  result->is_error_message();
}

}  // namespace

class DeviceAPIServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  DeviceAPIServiceTest() : account_id_(AccountId::FromUserEmail(kUserEmail)) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InstallTrustedApp();
    SetAllowedOrigin();
  }

  void InstallTrustedApp() {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    base::Value::Dict app_policy;
    app_policy.Set(web_app::kUrlKey, kDefaultAppInstallUrl);
    update->Append(std::move(app_policy));
  }

  void RemoveTrustedApp() {
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   base::Value::List());
  }

  void SetAllowedOrigin() {
    base::Value::List allowed_origins;
    allowed_origins.Append(kTrustedUrl);
    allowed_origins.Append(kKioskAppInstallUrl);
    profile()->GetPrefs()->SetList(prefs::kDeviceAttributesAllowedForOrigins,
                                   std::move(allowed_origins));
  }

  void RemoveAllowedOrigin() {
    profile()->GetPrefs()->SetList(prefs::kDeviceAttributesAllowedForOrigins,
                                   base::Value::List());
  }

  void TryCreatingService(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    DeviceServiceImpl::Create(main_rfh(), remote_.BindNewPipeAndPassReceiver());
  }

  void VerifyErrorMessageResultForAllDeviceAttributesAPIs() {
    remote()->get()->GetDirectoryId(base::BindOnce(VerifyErrorMessageResult));
    remote()->get()->GetHostname(base::BindOnce(VerifyErrorMessageResult));
    remote()->get()->GetSerialNumber(base::BindOnce(VerifyErrorMessageResult));
    remote()->get()->GetAnnotatedAssetId(
        base::BindOnce(VerifyErrorMessageResult));
    remote()->get()->GetAnnotatedLocation(
        base::BindOnce(VerifyErrorMessageResult));
    remote()->FlushForTesting();
  }

  const AccountId& account_id() const { return account_id_; }

  mojo::Remote<blink::mojom::DeviceAPIService>* remote() { return &remote_; }

 private:
  mojo::Remote<blink::mojom::DeviceAPIService> remote_;
  AccountId account_id_;
};

TEST_F(DeviceAPIServiceTest, EnableServiceByDefault) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceTest, ReportErrorForDefaultUser) {
  TryCreatingService(GURL(kTrustedUrl));
  VerifyErrorMessageResultForAllDeviceAttributesAPIs();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled in the Incognito mode.
TEST_F(DeviceAPIServiceTest, IncognitoProfile) {
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);
  TryCreatingService(GURL(kTrustedUrl));

  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class DeviceAPIServiceRegularUserTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceRegularUserTest()
      : fake_user_manager_(new user_manager::FakeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

  void LoginRegularUser(bool is_affiliated) {
    const user_manager::User* user =
        fake_user_manager()->AddUserWithAffiliation(account_id(),
                                                    is_affiliated);
    fake_user_manager()->UserLoggedIn(user->GetAccountId(),
                                      user->username_hash(), false, false);
  }

  user_manager::FakeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

 private:
  user_manager::FakeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForUnaffiliatedUser) {
  LoginRegularUser(false);
  TryCreatingService(GURL(kTrustedUrl));
  VerifyErrorMessageResultForAllDeviceAttributesAPIs();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForDisallowedOrigin) {
  LoginRegularUser(true);
  TryCreatingService(GURL(kTrustedUrl));
  RemoveAllowedOrigin();

  VerifyErrorMessageResultForAllDeviceAttributesAPIs();
  ASSERT_TRUE(remote()->is_connected());
}

class DeviceAPIServiceWithKioskUserTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceWithKioskUserTest()
      : fake_user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

  void SetUp() override {
    DeviceAPIServiceTest::SetUp();
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    app_manager_ = std::make_unique<ash::WebKioskAppManager>();
  }

  void TearDown() override {
    app_manager_.reset();
    DeviceAPIServiceTest::TearDown();
  }

  void LoginKioskUser() {
    app_manager()->AddAppForTesting(account_id(), GURL(kKioskAppInstallUrl));
    fake_user_manager()->AddWebKioskAppUser(account_id());
    fake_user_manager()->LoginUser(account_id());
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

  ash::WebKioskAppManager* app_manager() const { return app_manager_.get(); }

 private:
  ash::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<ash::WebKioskAppManager> app_manager_;
  base::test::ScopedCommandLine command_line_;
};

// The service should be enabled if the current origin is same as the origin of
// Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, EnableServiceForKioskOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, DisableServiceForInvalidOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kInvalidKioskAppUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app, even if it is trusted (force-installed).
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DisableServiceForNonKioskTrustedOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithFeatureFlagTest
    : public DeviceAPIServiceRegularUserTest {
 public:
  DeviceAPIServiceWithFeatureFlagTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableRestrictedWebApis);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DeviceAPIServiceWithFeatureFlagTest, ConnectsForTrustedApps) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWithFeatureFlagTest, DoesNotConnectForUntrustedApps) {
  TryCreatingService(GURL(kUntrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWithFeatureFlagTest, DisconnectWhenTrustRevoked) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  RemoveTrustedApp();
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithoutFeatureFlagTest
    : public DeviceAPIServiceRegularUserTest {
 public:
  DeviceAPIServiceWithoutFeatureFlagTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kEnableRestrictedWebApis);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DeviceAPIServiceWithoutFeatureFlagTest, DoesNotConnectWhenFlagOff) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

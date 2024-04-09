// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/common/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char kDefaultAppInstallUrl[] = "https://example.com/install";
constexpr char kTrustedUrl[] = "https://example.com/sample";
constexpr char kUntrustedUrl[] = "https://non-example.com/sample";
constexpr char kKioskAppInstallUrl[] = "https://kiosk.com/install";
constexpr char kUserEmail[] = "user-email@example.com";

constexpr char kNotAffiliatedErrorMessage[] =
    "This web API is not allowed if the current profile is not affiliated.";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kTrustedIwaAppId[] =
    "ggx2sheak3vpmm7vmjqnjwuzx3xwot3vdayrlgnvbkq2mp5lg4daaaic";
constexpr char kTrustedIwaAppOrigin[] =
    "isolated-app://ggx2sheak3vpmm7vmjqnjwuzx3xwot3vdayrlgnvbkq2mp5lg4daaaic";
constexpr char kUntrustedIwaAppOrigin[] =
    "isolated-app://abc2sheak3vpmm7vmjqnjwuzx3xwot3vdayrlgnvbkq2mp5lg4daaaic";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kKioskAppUrl[] = "https://kiosk.com/sample";
constexpr char kInvalidKioskAppUrl[] = "https://invalid-kiosk.com/sample";

constexpr char kNotAllowedOriginErrorMessage[] =
    "The current origin cannot use this web API because it is not allowed by "
    "the DeviceAttributesAllowedForOrigins policy.";
#endif

}  // namespace

class DeviceAPIServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  DeviceAPIServiceTest() : account_id_(AccountId::FromUserEmail(kUserEmail)) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InstallTrustedApps();
    SetAllowedOrigin();
  }

  virtual void InstallTrustedApps() {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    base::Value::Dict app_policy;
    app_policy.Set(web_app::kUrlKey, kDefaultAppInstallUrl);
    update->Append(std::move(app_policy));
  }

  virtual void RemoveTrustedApps() {
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

  void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      const std::string& expected_error_message) {
    base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

    remote()->get()->GetDirectoryId(future.GetRepeatingCallback());
    EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

    remote()->get()->GetHostname(future.GetRepeatingCallback());
    EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

    remote()->get()->GetSerialNumber(future.GetRepeatingCallback());
    EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

    remote()->get()->GetAnnotatedAssetId(future.GetRepeatingCallback());
    EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

    remote()->get()->GetAnnotatedLocation(future.GetRepeatingCallback());
    EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);
  }

  const AccountId& account_id() const { return account_id_; }

  mojo::Remote<blink::mojom::DeviceAPIService>* remote() { return &remote_; }

 private:
  mojo::Remote<blink::mojom::DeviceAPIService> remote_;
  AccountId account_id_;
};

TEST_F(DeviceAPIServiceTest, ConnectsForTrustedApps) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled in the Incognito mode.
TEST_F(DeviceAPIServiceTest, DoesNotConnectForIncognitoProfile) {
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);
  TryCreatingService(GURL(kTrustedUrl));

  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceTest, DoesNotConnectForUntrustedApps) {
  TryCreatingService(GURL(kUntrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceTest, DisconnectWhenTrustRevoked) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  RemoveTrustedApps();
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceTest, ReportErrorForDefaultUser) {
  TryCreatingService(GURL(kTrustedUrl));
  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAffiliatedErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

#if BUILDFLAG(IS_CHROMEOS)

class DeviceAPIServiceIwaTest : public DeviceAPIServiceTest {
 public:
  void InstallTrustedApps() override {
    DeviceAPIServiceTest::InstallTrustedApps();

    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kIsolatedWebAppInstallForceList);
    base::Value::Dict app_policy;
    app_policy.Set(web_app::kPolicyWebBundleIdKey, kTrustedIwaAppId);
    update->Append(std::move(app_policy));
  }

  void RemoveTrustedApps() override {
    DeviceAPIServiceTest::RemoveTrustedApps();

    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   base::Value::List());
  }
};

TEST_F(DeviceAPIServiceIwaTest, ConnectsForTrustedApps) {
  TryCreatingService(GURL(kTrustedIwaAppOrigin));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceIwaTest, DoesNotConnectForUntrustedApps) {
  TryCreatingService(GURL(kUntrustedIwaAppOrigin));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceIwaTest, DisconnectWhenTrustRevoked) {
  TryCreatingService(GURL(kTrustedIwaAppOrigin));
  remote()->FlushForTesting();
  RemoveTrustedApps();
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceIwaTest, ReportErrorForDefaultUser) {
  TryCreatingService(GURL(kTrustedIwaAppOrigin));
  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAffiliatedErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)

class DeviceAPIServiceRegularUserTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceRegularUserTest() = default;

  void LoginRegularUser(bool is_affiliated) {
    fake_user_manager_ = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    const user_manager::User* user =
        fake_user_manager()->AddUserWithAffiliation(account_id(),
                                                    is_affiliated);
    fake_user_manager()->UserLoggedIn(user->GetAccountId(),
                                      user->username_hash(), false, false);
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

 private:
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
};

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForUnaffiliatedUser) {
  LoginRegularUser(false);
  TryCreatingService(GURL(kTrustedUrl));
  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAffiliatedErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForDisallowedOrigin) {
  LoginRegularUser(true);
  TryCreatingService(GURL(kTrustedUrl));
  RemoveAllowedOrigin();

  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAllowedOriginErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

class DeviceAPIServiceWithKioskUserTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceWithKioskUserTest()
      : fake_user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_.get())) {}

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

  void LoginChromeAppKioskUser() {
    fake_user_manager()->AddKioskAppUser(account_id());
    fake_user_manager()->LoginUser(account_id());
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

  ash::WebKioskAppManager* app_manager() const { return app_manager_.get(); }

 private:
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<ash::WebKioskAppManager> app_manager_;
  base::test::ScopedCommandLine command_line_;
};

// The service should be enabled if the current origin is same as the origin of
// Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, ConnectsForKioskOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, DoesNotConnectForInvalidOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kInvalidKioskAppUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app, even if it is trusted (force-installed).
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DoesNotConnectForNonKioskTrustedOrigin) {
  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if a non-PWA kiosk user is logged in.
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DoesNotConnectForChromeAppKioskSession) {
  LoginChromeAppKioskUser();

  TryCreatingService(GURL(kKioskAppUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

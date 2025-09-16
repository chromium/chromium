// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kDefaultAppInstallUrl[] = "https://example.com/install";
constexpr char kTrustedUrl[] = "https://example.com/sample";
constexpr char kUntrustedUrl[] = "https://non-example.com/sample";
constexpr char kUserEmail[] = "user-email@example.com";
constexpr char kNotAffiliatedErrorMessage[] =
    "This web API is not allowed if the current profile is not affiliated.";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kKioskAppInstallUrl[] = "https://kiosk.com/install";
constexpr char kKioskAppUrl[] = "https://kiosk.com/sample";
constexpr char kInvalidKioskAppUrl[] = "https://invalid-kiosk.com/sample";
constexpr char kNoDeviceAttributesPermissionErrorMessage[] =
    "The current origin cannot use this web API because it was not granted the "
    "'device-attributes' permission.";
constexpr char kPermissionsPolicyMojoErrorMessage[] =
    "Permissions policy blocks access to Device Attributes.";
#endif

}  // namespace

namespace {

using Result = blink::mojom::DeviceAttributeResult;

constexpr char kAnnotatedAssetId[] = "annotated_asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kDirectoryApiId[] = "directory_api_id";
constexpr char kHostname[] = "hostname";
constexpr char kSerialNumber[] = "serial_number";

class FakeDeviceAttributeApi : public DeviceAttributeApi {
 public:
  FakeDeviceAttributeApi() = default;
  ~FakeDeviceAttributeApi() override = default;

  // This method forwards calls to DeviceAttributesApiImpl to the test the
  // actual error reported by the service.
  void ReportNotAllowedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback)
      override {
    device_attributes_api_.ReportNotAllowedError(std::move(callback));
  }

  // This method forwards calls to DeviceAttributesApiImpl to the test the
  // actual error reported by the service.
  void ReportNotAffiliatedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback)
      override {
    device_attributes_api_.ReportNotAffiliatedError(std::move(callback));
  }

  void GetDirectoryId(blink::mojom::DeviceAPIService::GetDirectoryIdCallback
                          callback) override {
    std::move(callback).Run(Result::NewAttribute(kDirectoryApiId));
  }

  void GetHostname(
      blink::mojom::DeviceAPIService::GetHostnameCallback callback) override {
    std::move(callback).Run(Result::NewAttribute(kHostname));
  }

  void GetSerialNumber(blink::mojom::DeviceAPIService::GetSerialNumberCallback
                           callback) override {
    std::move(callback).Run(Result::NewAttribute(kSerialNumber));
  }

  void GetAnnotatedAssetId(
      blink::mojom::DeviceAPIService::GetAnnotatedAssetIdCallback callback)
      override {
    std::move(callback).Run(Result::NewAttribute(kAnnotatedAssetId));
  }

  void GetAnnotatedLocation(
      blink::mojom::DeviceAPIService::GetAnnotatedLocationCallback callback)
      override {
    std::move(callback).Run(Result::NewAttribute(kAnnotatedLocation));
  }

 private:
  DeviceAttributeApiImpl device_attributes_api_;
};
}  // namespace

class DeviceAPIServiceTest {
 public:
  void InstallTrustedApps(Profile* profile) {
    app_id_ = web_app::test::InstallDummyWebApp(
        profile, "Policy installed app", GURL(kDefaultAppInstallUrl),
        webapps::WebappInstallSource::EXTERNAL_POLICY);
  }

  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api,
      content::WebContents* web_contents) {
    // Isolated Web Apps require Cross Origin Isolation headers to be included
    // in the response.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    if (url.SchemeIs(webapps::kIsolatedAppScheme)) {
      web_app::SimulateIsolatedWebAppNavigation(web_contents, url);
    } else {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                                 url);
    }
#else   //!(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        //! BUILDFLAG(IS_CHROMEOS))
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               url);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    DeviceServiceImpl::CreateForTest(web_contents->GetPrimaryMainFrame(),
                                     remote()->BindNewPipeAndPassReceiver(),
                                     std::move(device_attribute_api));
  }

  const webapps::AppId& app_id() const { return *app_id_; }

  mojo::Remote<blink::mojom::DeviceAPIService>* remote() { return &remote_; }

 private:
  std::optional<webapps::AppId> app_id_;
  mojo::Remote<blink::mojom::DeviceAPIService> remote_;
};

namespace {
void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
    blink::mojom::DeviceAPIService* service,
    const std::string& expected_error_message) {
  base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

  service->GetDirectoryId(future.GetCallback());
  EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

  service->GetHostname(future.GetCallback());
  EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

  service->GetSerialNumber(future.GetCallback());
  EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

  service->GetAnnotatedAssetId(future.GetCallback());
  EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);

  service->GetAnnotatedLocation(future.GetCallback());
  EXPECT_EQ(future.Take()->get_error_message(), expected_error_message);
}
}  // namespace

class DeviceAPIServiceWebAppTest : public DeviceAPIServiceTest,
                                   public WebAppTest {
 public:
  DeviceAPIServiceWebAppTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {
    account_id_ = AccountId::FromUserEmail(kUserEmail);
  }

  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    InstallTrustedApps();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(profile()));
#if BUILDFLAG(IS_CHROMEOS)
    SetAllowedOrigin();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void InstallTrustedApps() {
    DeviceAPIServiceTest::InstallTrustedApps(profile());
  }

  void UninstallAllApps() { web_app::test::UninstallAllWebApps(profile()); }

  webapps::AppId UserInstallWebApp() {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL(kDefaultAppInstallUrl));

    return web_app::test::InstallWebApp(
        profile(), std::move(app_info),
        /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetAllowedOrigin() {
    base::Value::List allowed_origins;
    allowed_origins.Append(kTrustedUrl);
    allowed_origins.Append(kKioskAppInstallUrl);
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDeviceAttributesAllowedForOrigins,
        std::move(allowed_origins));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
    DeviceAPIServiceTest::TryCreatingService(
        url, std::move(device_attribute_api), web_contents());
  }

  void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      const std::string& expected_error_message) {
    ::VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        remote()->get(), expected_error_message);
  }

  const AccountId& account_id() const { return account_id_; }

  web_app::WebAppProvider* provider() {
    return web_app::WebAppProvider::GetForTest(profile());
  }

  void TearDown() override {
    provider()->Shutdown();
    WebAppTest::TearDown();
  }

  void EnableFeature(const base::Feature& param) {
    feature_list_.InitAndEnableFeature(param);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_list_.InitAndDisableFeature(feature);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  AccountId account_id_;
};

TEST_F(DeviceAPIServiceWebAppTest, ConnectsForTrustedApps) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled in the Incognito mode.
TEST_F(DeviceAPIServiceWebAppTest, DoesNotConnectForIncognitoProfile) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());

  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, DoesNotConnectForUntrustedApps) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kUntrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, DisconnectWhenTrustRevoked) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  UninstallAllApps();
  remote()->FlushForTesting();

  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, MultiOriginDisconnectWhenTrustRevoked) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  webapps::AppId app_id = UserInstallWebApp();

  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  UninstallAllApps();
  remote()->FlushForTesting();

  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, ReportErrorForDefaultUser) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAffiliatedErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest,
       DoesNotConnectForTrustedAppsWithFeatureFlagEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest,
       DoesNotConnectForIncognitoProfileWithFeatureFlagEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest,
       DoesNotConnectForUntrustedAppsWithFeatureFlagEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  TryCreatingService(GURL(kUntrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

#if BUILDFLAG(IS_CHROMEOS)

class DeviceAPIServiceIwaTest
    : public DeviceAPIServiceTest,
      public web_app::IsolatedWebAppTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  void SetUp() override {
    web_app::IsolatedWebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(profile()));
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), /*instance=*/nullptr);
  }

  void TearDown() override {
    web_contents_.reset();
    rvh_test_enabler_.reset();
    web_app::IsolatedWebAppTest::TearDown();
  }

  void SetAllowedOrigin(const std::string& origin) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDeviceAttributesAllowedForOrigins,
        base::Value::List().Append(origin));
  }

  void SetBlockedOrigin(const std::string& origin) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDeviceAttributesBlockedForOrigins,
        base::Value::List().Append(origin));
  }

  void SetEnterprisePoliciesForOrigin(const std::string& origin) {
    if (IsBlockPolicySet()) {
      SetBlockedOrigin(origin);
    }
    if (IsAllowPolicySet()) {
      SetAllowedOrigin(origin);
    }
  }

  web_app::IsolatedWebAppUrlInfo InstallTrustedIWA() {
    return InstallIWA(/*trusted=*/true);
  }

  web_app::IsolatedWebAppUrlInfo InstallUntrustedIWA() {
    return InstallIWA(/*trusted=*/false);
  }

  void ForceUninstall(const web_app::IsolatedWebAppUrlInfo& url_info) {
    base::RunLoop run_loop;
    auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
            run_loop.Quit();
          }
          std::move(callback).Run();
        }));

    base::test::TestFuture<webapps::UninstallResultCode> future;
    provider().scheduler().RemoveInstallManagementMaybeUninstall(
        url_info.app_id(), web_app::WebAppManagement::Type::kIwaPolicy,
        webapps::WebappUninstallSource::kIwaEnterprisePolicy,
        future.GetCallback());
    auto code = future.Get();
    ASSERT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
    run_loop.Run();
  }

  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
    DeviceAPIServiceTest::TryCreatingService(
        url, std::move(device_attribute_api), web_contents_.get());
  }

  void InitWebContents() {}

  void EnableFeature(const base::Feature& param) {
    feature_list_.InitAndEnableFeature(param);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_list_.InitAndDisableFeature(feature);
  }

  void SetDeviceAttributesPermissionPolicyFeatureFlag() {
    if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()) {
      EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
    } else {
      DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
    }
  }

  bool IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() {
    return std::get<0>(GetParam());
  }
  bool IsAllowPolicySet() { return std::get<1>(GetParam()); }
  bool IsBlockPolicySet() { return std::get<2>(GetParam()); }
  bool IsPermissionsPolicyGranted() { return std::get<3>(GetParam()); }

 private:
  web_app::IsolatedWebAppUrlInfo InstallIWA(bool trusted) {
    auto manifest_builder = web_app::ManifestBuilder();
    if (IsPermissionsPolicyGranted()) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kDeviceAttributes, true,
          {});
    }
    const std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
        web_app::IsolatedWebAppBuilder(manifest_builder).BuildBundle();
    bundle->TrustSigningKey();
    if (trusted) {
      return bundle
          ->InstallWithSource(
              profile(),
              &web_app::IsolatedWebAppInstallSource::FromExternalPolicy)
          .value();
    } else {
      return bundle->InstallChecked(profile());
    }
  }

  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(DeviceAPIServiceIwaTest, CheckTrustedApps) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetDeviceAttributesPermissionPolicyFeatureFlag();

  auto url_info = InstallTrustedIWA();
  bool should_connect = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                            ? IsPermissionsPolicyGranted()
                            : true;
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  if (should_connect) {
    ASSERT_TRUE(remote()->is_connected());
  } else {
    ASSERT_FALSE(remote()->is_connected());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Permissions policy blocks access to Device Attributes.");
  }
}

TEST_P(DeviceAPIServiceIwaTest, CheckUntrustedApps) {
  SetDeviceAttributesPermissionPolicyFeatureFlag();
  auto url_info = InstallUntrustedIWA();
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_P(DeviceAPIServiceIwaTest, CheckTrustRevoked) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  SetDeviceAttributesPermissionPolicyFeatureFlag();
  auto url_info = InstallTrustedIWA();
  bool should_connect = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                            ? IsPermissionsPolicyGranted()
                            : true;
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  if (should_connect) {
    ForceUninstall(url_info);
    remote()->FlushForTesting();
    ASSERT_FALSE(remote()->is_connected());
  } else {
    ASSERT_FALSE(remote()->is_connected());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              kPermissionsPolicyMojoErrorMessage);
  }
}

TEST_P(DeviceAPIServiceIwaTest, CheckErrorForDefaultUser) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  bool should_connect = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                            ? IsPermissionsPolicyGranted()
                            : true;
  SetDeviceAttributesPermissionPolicyFeatureFlag();
  auto url_info = InstallTrustedIWA();
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  if (should_connect) {
    VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        remote()->get(), kNotAffiliatedErrorMessage);
    ASSERT_TRUE(remote()->is_connected());
  } else {
    ASSERT_FALSE(remote()->is_connected());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              kPermissionsPolicyMojoErrorMessage);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAPIServiceIwaTest,
    ::testing::Combine(
        ::testing::Bool(),  // kDeviceAttributesPermissionPolicy feature flag
        ::testing::Bool(),  // allow policy
        ::testing::Bool(),  // block policy
        ::testing::Bool()   // permissions policy
        ),
    [](const ::testing::TestParamInfo<std::tuple<bool, bool, bool, bool>>&
           info) {
      return base::StringPrintf(
          "FeatureFlag%s_AllowPolicy%s_BlockPolicy%s_PermissionsPolicy%s",
          std::get<0>(info.param) ? "Enabled" : "Disabled",
          std::get<1>(info.param) ? "Set" : "Unset",
          std::get<2>(info.param) ? "Set" : "Unset",
          std::get<3>(info.param) ? "Granted" : "Denied");
    });

class DeviceAPIServiceParamTest
    : public DeviceAPIServiceWebAppTest,
      public testing::WithParamInterface<std::pair<std::string, bool>> {
 public:
  void SetAllowedOriginFromParam() { SetAllowedOrigin(GetParamOrigin()); }

  void SetAllowedOrigin(const std::string& origin) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDeviceAttributesAllowedForOrigins,
        base::Value::List().Append(origin));
  }

  void AllowOriginsByDefault() {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDefaultDeviceAttributesSetting,
        base::Value(kAllowSetting));
  }

  void BlockOriginsByDefault() {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDefaultDeviceAttributesSetting,
        base::Value(kBlockSetting));
  }

  void EnableFeatureAndAllowlistOrigin(const base::Feature& param,
                                       const std::string& origin) {
    base::FieldTrialParams feature_params;
    feature_params[permissions::feature_params::
                       kWebKioskBrowserPermissionsAllowlist.name] = origin;
    feature_list_.InitAndEnableFeatureWithParameters(param, feature_params);
  }

  void InitWithFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void InitWithFeaturesAndParameters(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetKioskBrowserPermissionsAllowedForOrigins(const std::string& origin) {
    profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(std::move(origin)));
  }

  void VerifyCanAccessForAllDeviceAttributesAPIs() {
    base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

    remote()->get()->GetDirectoryId(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kDirectoryApiId);

    remote()->get()->GetHostname(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kHostname);

    remote()->get()->GetSerialNumber(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kSerialNumber);

    remote()->get()->GetAnnotatedAssetId(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedAssetId);

    remote()->get()->GetAnnotatedLocation(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedLocation);
  }

  const std::string& GetParamOrigin() { return GetParam().first; }

  bool ExpectApiAvailable() { return GetParam().second; }

 private:
  static constexpr int32_t kAllowSetting = 1;
  static constexpr int32_t kBlockSetting = 2;
};

class DeviceAPIServiceRegularUserTest : public DeviceAPIServiceParamTest {
 public:
  void LoginRegularUser(bool is_affiliated) {
    const user_manager::User* user =
        fake_user_manager()->AddUserWithAffiliation(account_id(),
                                                    is_affiliated);
    fake_user_manager()->UserLoggedIn(
        user->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void RemoveAllowedOrigin() {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDeviceAttributesAllowedForOrigins, base::Value::List());
  }

  void TearDown() override {
    provider()->Shutdown();
    DeviceAPIServiceParamTest::TearDown();
  }
};

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForUnaffiliatedUser) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginRegularUser(false);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNotAffiliatedErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceRegularUserTest, ReportErrorForDisallowedOrigin) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginRegularUser(true);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  RemoveAllowedOrigin();

  VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      kNoDeviceAttributesPermissionErrorMessage);
  ASSERT_TRUE(remote()->is_connected());
}

TEST_P(DeviceAPIServiceRegularUserTest, TestPolicyOriginPatterns) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  SetAllowedOriginFromParam();
  LoginRegularUser(true);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());

  if (ExpectApiAvailable()) {
    VerifyCanAccessForAllDeviceAttributesAPIs();
  } else {
    VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        kNoDeviceAttributesPermissionErrorMessage);
  }
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceRegularUserTest,
       DoesNotConnectForUnaffiliatedUserWithFeatureFlag) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginRegularUser(false);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceRegularUserTest,
       DoesNotConnectForAffiliatedUserWithFeatureFlag) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginRegularUser(true);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAPIServiceRegularUserTest,
    testing::ValuesIn(
        {std::pair<std::string, bool>("*", false),
         std::pair<std::string, bool>(".example.com", false),
         std::pair<std::string, bool>("example.", false),
         std::pair<std::string, bool>("file://example*", false),
         std::pair<std::string, bool>("invalid-example.com", false),
         std::pair<std::string, bool>(kTrustedUrl, true),
         std::pair<std::string, bool>("https://example.com", true),
         std::pair<std::string, bool>("https://example.com/sample", true),
         std::pair<std::string, bool>("example.com", true),
         std::pair<std::string, bool>("*://example.com:*/", true),
         std::pair<std::string, bool>("[*.]example.com", true)}));

class DeviceAPIServiceRegularUserIwaTest : public DeviceAPIServiceIwaTest {
 public:
  DeviceAPIServiceRegularUserIwaTest() {
    account_id_ = AccountId::FromUserEmail(kUserEmail);
  }

  void LoginRegularUser(bool is_affiliated) {
    const user_manager::User* user =
        fake_user_manager()->AddUserWithAffiliation(account_id(),
                                                    is_affiliated);
    fake_user_manager()->UserLoggedIn(
        user->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
  const AccountId& account_id() const { return account_id_; }

  void AllowOriginsByDefault() {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDefaultDeviceAttributesSetting,
        base::Value(kAllowSetting));
  }

  void BlockOriginsByDefault() {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDefaultDeviceAttributesSetting,
        base::Value(kBlockSetting));
  }

  void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      const std::string& expected_error_message) {
    ::VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        remote()->get(), expected_error_message);
  }

  void VerifyCanAccessForAllDeviceAttributesAPIs() {
    base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

    remote()->get()->GetDirectoryId(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kDirectoryApiId);

    remote()->get()->GetHostname(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kHostname);

    remote()->get()->GetSerialNumber(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kSerialNumber);

    remote()->get()->GetAnnotatedAssetId(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedAssetId);

    remote()->get()->GetAnnotatedLocation(future.GetCallback());
    EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedLocation);
  }

 private:
  static constexpr int32_t kAllowSetting = 1;
  static constexpr int32_t kBlockSetting = 2;
  AccountId account_id_;
};

TEST_P(DeviceAPIServiceRegularUserIwaTest,
       CheckTrustedAppsForUnaffiliatedUser) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetDeviceAttributesPermissionPolicyFeatureFlag();

  LoginRegularUser(false);
  auto url_info = InstallTrustedIWA();
  SetEnterprisePoliciesForOrigin(url_info.origin().Serialize());
  bool should_connect = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                            ? IsPermissionsPolicyGranted()
                            : true;
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  if (should_connect) {
    ASSERT_TRUE(remote()->is_connected());
    VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        kNotAffiliatedErrorMessage);
  } else {
    ASSERT_FALSE(remote()->is_connected());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              kPermissionsPolicyMojoErrorMessage);
  }
}

TEST_P(DeviceAPIServiceRegularUserIwaTest, CheckTrustedAppsForAffiliatedUser) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetDeviceAttributesPermissionPolicyFeatureFlag();

  LoginRegularUser(true);
  auto url_info = InstallTrustedIWA();
  SetEnterprisePoliciesForOrigin(url_info.origin().Serialize());
  bool should_connect = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                            ? IsPermissionsPolicyGranted()
                            : true;
  bool should_work = IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
                         ? IsPermissionsPolicyGranted() && !IsBlockPolicySet()
                         : IsAllowPolicySet();
  TryCreatingService(url_info.origin().GetURL(),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  if (should_connect) {
    ASSERT_TRUE(remote()->is_connected());
    if (should_work) {
      VerifyCanAccessForAllDeviceAttributesAPIs();
    } else {
      VerifyErrorMessageResultForAllDeviceAttributesAPIs(
          kNoDeviceAttributesPermissionErrorMessage);
    }
  } else {
    ASSERT_FALSE(remote()->is_connected());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              kPermissionsPolicyMojoErrorMessage);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAPIServiceRegularUserIwaTest,
    ::testing::Combine(
        ::testing::Bool(),  // kDeviceAttributesPermissionPolicy feature flag
        ::testing::Bool(),  // allow policy
        ::testing::Bool(),  // block policy
        ::testing::Bool()   // permissions policy
        ),
    [](const ::testing::TestParamInfo<std::tuple<bool, bool, bool, bool>>&
           info) {
      return base::StringPrintf(
          "FeatureFlag%s_AllowPolicy%s_BlockPolicy%s_PermissionsPolicy%s",
          std::get<0>(info.param) ? "Enabled" : "Disabled",
          std::get<1>(info.param) ? "Set" : "Unset",
          std::get<2>(info.param) ? "Set" : "Unset",
          std::get<3>(info.param) ? "Granted" : "Denied");
    });

class DeviceAPIServiceWithKioskUserTest : public DeviceAPIServiceParamTest {
 public:
  DeviceAPIServiceWithKioskUserTest() {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void SetUp() override {
    DeviceAPIServiceParamTest::SetUp();
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    app_manager_ = std::make_unique<ash::KioskWebAppManager>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
        &kiosk_cryptohome_remover_);
  }

  void TearDown() override {
    app_manager_.reset();
    DeviceAPIServiceParamTest::TearDown();
  }

  void LoginKioskUser() {
    app_manager()->AddAppForTesting(account_id(), GURL(kKioskAppInstallUrl));
    fake_user_manager()->AddKioskWebAppUser(account_id());
    fake_user_manager()->LoginUser(account_id());
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_.Get();
  }

  ash::KioskWebAppManager* app_manager() const { return app_manager_.get(); }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  ash::KioskCryptohomeRemover kiosk_cryptohome_remover_{
      TestingBrowserProcess::GetGlobal()->local_state()};
  std::unique_ptr<ash::KioskWebAppManager> app_manager_;
  base::test::ScopedCommandLine command_line_;
};

// The service should be enabled if the current origin is same as the origin of
// Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, ConnectsForKioskOrigin) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest, DoesNotConnectForInvalidOrigin) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kInvalidKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app, even if it is trusted (force-installed).
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DoesNotConnectForNonKioskTrustedOrigin) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be enabled if the current origin is same as the origin of
// Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest,
       ConnectsForKioskOriginWithFeatureEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app.
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DoesNotConnectForInvalidOriginWithFeatureEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kInvalidKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if the current origin is different from the
// origin of Kiosk app, even if it is trusted (force-installed).
TEST_F(DeviceAPIServiceWithKioskUserTest,
       DoesNotConnectForNonKioskTrustedOriginWithFeatureEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithChromeAppKioskUserTest
    : public DeviceAPIServiceTest,
      public ChromeRenderViewHostTestHarness {
 public:
  DeviceAPIServiceWithChromeAppKioskUserTest()
      : account_id_(AccountId::FromUserEmail(kUserEmail)) {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void LoginChromeAppKioskUser() {
    fake_user_manager()->AddKioskChromeAppUser(account_id());
    fake_user_manager()->LoginUser(account_id());
  }

  const AccountId& account_id() const { return account_id_; }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_.Get();
  }

  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
    DeviceAPIServiceTest::TryCreatingService(
        url, std::move(device_attribute_api), web_contents());
  }

  void EnableFeature(const base::Feature& param) {
    feature_list_.InitAndEnableFeature(param);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_list_.InitAndDisableFeature(feature);
  }

 private:
  AccountId account_id_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  base::test::ScopedFeatureList feature_list_;
};

// The service should be disabled if a non-PWA kiosk user is logged in.
TEST_F(DeviceAPIServiceWithChromeAppKioskUserTest,
       DoesNotConnectForChromeAppKioskSession) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginChromeAppKioskUser();

  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

// The service should be disabled if a non-PWA kiosk user is logged in.
TEST_F(DeviceAPIServiceWithChromeAppKioskUserTest,
       DoesNotConnectForChromeAppKioskSessionWithFeatureFlagEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  LoginChromeAppKioskUser();

  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithKioskUserTestForOrigins
    : public DeviceAPIServiceWithKioskUserTest {};

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestTrustedKioskOriginsWhenEnabledByFeature) {
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          kTrustedUrl;
  InitWithFeaturesAndParameters(
      /*enabled_features=*/{{permissions::features::
                                 kAllowMultipleOriginsForWebKioskPermissions,
                             feature_params}},
      /*disabled_features=*/{
          blink::features::kDeviceAttributesPermissionPolicy});

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestUntrustedKioskOriginsWhenEnabledByFeature) {
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          kTrustedUrl;
  InitWithFeaturesAndParameters(
      /*enabled_features=*/{{permissions::features::
                                 kAllowMultipleOriginsForWebKioskPermissions,
                             feature_params}},
      /*disabled_features=*/{
          blink::features::kDeviceAttributesPermissionPolicy});

  LoginKioskUser();
  TryCreatingService(GURL(kUntrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestTrustedKioskOriginWhenMultipleOriginPrefIsSet) {
  InitWithFeatures(
      /*enabled_features=*/{permissions::features::
                                kAllowMultipleOriginsForWebKioskPermissions},
      /*disabled_features=*/{
          blink::features::kDeviceAttributesPermissionPolicy});
  SetKioskBrowserPermissionsAllowedForOrigins(kTrustedUrl);

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestKioskInstallOriginWhenMultipleOriginPrefIsNotSet) {
  InitWithFeatures(
      /*enabled_features=*/{permissions::features::
                                kAllowMultipleOriginsForWebKioskPermissions},
      /*disabled_features=*/{
          blink::features::kDeviceAttributesPermissionPolicy});

  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppInstallUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for install origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestMultipleOriginPolicyWhenFeatureIsDisabled) {
  InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          blink::features::kDeviceAttributesPermissionPolicy,
          permissions::features::kAllowMultipleOriginsForWebKioskPermissions});
  SetKioskBrowserPermissionsAllowedForOrigins(kTrustedUrl);

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check that the service is not able to connect when the feature is disabled.
  ASSERT_FALSE(remote()->is_connected());
}

TEST_P(DeviceAPIServiceWithKioskUserTestForOrigins, TestPolicyOriginPatterns) {
  DisableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  BlockOriginsByDefault();
  SetAllowedOriginFromParam();
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<FakeDeviceAttributeApi>());

  remote()->FlushForTesting();

  ASSERT_TRUE(remote()->is_connected());

  if (ExpectApiAvailable()) {
    VerifyCanAccessForAllDeviceAttributesAPIs();
  } else {
    VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        kNoDeviceAttributesPermissionErrorMessage);
  }
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestTrustedKioskOriginsWhenEnabledByFeatureWithDAPPFeatureEnabled) {
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          kTrustedUrl;
  InitWithFeaturesAndParameters(
      /*enabled_features=*/{{permissions::features::
                                 kAllowMultipleOriginsForWebKioskPermissions,
                             feature_params},
                            {blink::features::kDeviceAttributesPermissionPolicy,
                             {}}},
      /*disabled_features=*/{});

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestUntrustedKioskOriginsWhenEnabledByFeatureWithDAPPFeatureEnabled) {
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          kTrustedUrl;
  InitWithFeaturesAndParameters(
      /*enabled_features=*/{{permissions::features::
                                 kAllowMultipleOriginsForWebKioskPermissions,
                             feature_params},
                            {blink::features::kDeviceAttributesPermissionPolicy,
                             {}}},
      /*disabled_features=*/{});

  LoginKioskUser();
  TryCreatingService(GURL(kUntrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(
    DeviceAPIServiceWithKioskUserTestForOrigins,
    TestTrustedKioskOriginWhenMultipleOriginPrefIsSetWithDAPPFeatureEnabled) {
  InitWithFeatures(
      /*enabled_features=*/{permissions::features::
                                kAllowMultipleOriginsForWebKioskPermissions,
                            blink::features::kDeviceAttributesPermissionPolicy},
      /*disabled_features=*/{});
  SetKioskBrowserPermissionsAllowedForOrigins(kTrustedUrl);

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for a different allowed origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(
    DeviceAPIServiceWithKioskUserTestForOrigins,
    TestKioskInstallOriginWhenMultipleOriginPrefIsNotSetWithDAPPFeatureEnabled) {
  InitWithFeatures(
      /*enabled_features=*/{permissions::features::
                                kAllowMultipleOriginsForWebKioskPermissions,
                            blink::features::kDeviceAttributesPermissionPolicy},
      /*disabled_features=*/{});

  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppInstallUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check whether the service connects for install origin.
  ASSERT_TRUE(remote()->is_connected());
  VerifyCanAccessForAllDeviceAttributesAPIs();
}

TEST_F(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestMultipleOriginPolicyWhenFeatureIsDisabledWithDAPPFeatureEnabled) {
  InitWithFeatures(
      /*enabled_features=*/{blink::features::kDeviceAttributesPermissionPolicy},
      /*disabled_features=*/{
          permissions::features::kAllowMultipleOriginsForWebKioskPermissions});
  SetKioskBrowserPermissionsAllowedForOrigins(kTrustedUrl);

  LoginKioskUser();
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<FakeDeviceAttributeApi>());
  remote()->FlushForTesting();

  // Check that the service is not able to connect when the feature is disabled.
  ASSERT_FALSE(remote()->is_connected());
}

TEST_P(DeviceAPIServiceWithKioskUserTestForOrigins,
       TestPolicyOriginPatternsWithDAPPFeatureEnabled) {
  EnableFeature(blink::features::kDeviceAttributesPermissionPolicy);
  BlockOriginsByDefault();
  SetAllowedOriginFromParam();
  LoginKioskUser();
  TryCreatingService(GURL(kKioskAppUrl),
                     std::make_unique<FakeDeviceAttributeApi>());

  remote()->FlushForTesting();

  ASSERT_TRUE(remote()->is_connected());

  if (ExpectApiAvailable()) {
    VerifyCanAccessForAllDeviceAttributesAPIs();
  } else {
    VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        kNoDeviceAttributesPermissionErrorMessage);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAPIServiceWithKioskUserTestForOrigins,
    testing::ValuesIn({std::pair<std::string, bool>("*", false),
                       std::pair<std::string, bool>("*.kiosk.com", false),
                       std::pair<std::string, bool>("*kiosk.com", false),
                       std::pair<std::string, bool>("kiosk.", false),
                       std::pair<std::string, bool>(kInvalidKioskAppUrl, false),
                       std::pair<std::string, bool>(kKioskAppUrl, true),
                       std::pair<std::string, bool>("https://kiosk.com", true),
                       std::pair<std::string, bool>("https://kiosk.com/sample",
                                                    true),
                       std::pair<std::string, bool>("kiosk.com", true),
                       std::pair<std::string, bool>("*://kiosk.com:*/", true),
                       std::pair<std::string, bool>("[*.]kiosk.com", true)}));
#endif  // BUILDFLAG(IS_CHROMEOS)

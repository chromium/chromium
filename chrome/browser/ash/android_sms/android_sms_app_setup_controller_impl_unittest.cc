// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_app_setup_controller_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace ash {
namespace android_sms {

namespace {

const char kTestUrl1[] = "https://test-url-1.com/";
const char kTestInstallUrl1[] = "https://test-url-1.com/install";
const char kTestUrl2[] = "https://test-url-2.com/";

web_app::ExternalInstallOptions GetInstallOptionsForUrl(const GURL& url) {
  web_app::ExternalInstallOptions options(
      url, web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kInternalDefault);
  options.override_previous_user_uninstall = true;
  options.require_manifest = true;
  return options;
}

class FakeCookieManager : public network::TestCookieManager {
 public:
  FakeCookieManager() = default;
  ~FakeCookieManager() override {
    EXPECT_TRUE(set_canonical_cookie_calls_.empty());
    EXPECT_TRUE(delete_cookies_calls_.empty());
  }

  void InvokePendingSetCanonicalCookieCallback(
      const std::string& expected_cookie_name,
      const std::string& expected_cookie_value,
      const GURL& expected_source_url,
      bool expected_modify_http_only,
      net::CookieOptions::SameSiteCookieContext expect_same_site_context,
      bool success) {
    ASSERT_FALSE(set_canonical_cookie_calls_.empty());
    auto params = std::move(set_canonical_cookie_calls_.front());
    set_canonical_cookie_calls_.erase(set_canonical_cookie_calls_.begin());

    EXPECT_EQ(expected_cookie_name, std::get<0>(params).Name());
    EXPECT_EQ(expected_cookie_value, std::get<0>(params).Value());
    EXPECT_EQ(expected_source_url, std::get<1>(params));
    EXPECT_EQ(expected_modify_http_only,
              !std::get<2>(params).exclude_httponly());
    EXPECT_EQ(expect_same_site_context,
              std::get<2>(params).same_site_cookie_context());
    net::CookieAccessResult access_result;

    if (!success) {
      access_result.status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
    }

    std::move(std::get<3>(params)).Run(access_result);
  }

  void InvokePendingDeleteCookiesCallback(
      const GURL& expected_url,
      const std::string& expected_cookie_name,
      bool success) {
    ASSERT_FALSE(delete_cookies_calls_.empty());
    auto params = std::move(delete_cookies_calls_.front());
    delete_cookies_calls_.erase(delete_cookies_calls_.begin());

    EXPECT_EQ(expected_url, params.first->url);
    EXPECT_EQ(expected_cookie_name, params.first->cookie_name);

    std::move(params.second).Run(success);
  }

  // network::mojom::CookieManager
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& options,
                          SetCanonicalCookieCallback callback) override {
    set_canonical_cookie_calls_.emplace_back(cookie, source_url, options,
                                             std::move(callback));
  }

  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {
    delete_cookies_calls_.emplace_back(std::move(filter), std::move(callback));
  }

 private:
  std::vector<std::tuple<net::CanonicalCookie,
                         GURL,
                         net::CookieOptions,
                         SetCanonicalCookieCallback>>
      set_canonical_cookie_calls_;
  std::vector<
      std::pair<network::mojom::CookieDeletionFilterPtr, DeleteCookiesCallback>>
      delete_cookies_calls_;
};

}  // namespace

class AndroidSmsAppSetupControllerImplTest : public testing::Test {
 public:
  AndroidSmsAppSetupControllerImplTest(
      const AndroidSmsAppSetupControllerImplTest&) = delete;
  AndroidSmsAppSetupControllerImplTest& operator=(
      const AndroidSmsAppSetupControllerImplTest&) = delete;

 protected:
  class TestPwaDelegate : public AndroidSmsAppSetupControllerImpl::PwaDelegate {
   public:
    explicit TestPwaDelegate(FakeCookieManager* fake_cookie_manager)
        : fake_cookie_manager_(fake_cookie_manager) {}
    ~TestPwaDelegate() override = default;

    void SetHasPwa(const GURL& url) {
      // If a PWA already exists for this URL, there is nothing to do.
      if (base::Contains(url_to_pwa_map_, url))
        return;

      url_to_pwa_map_[url] =
          web_app::GenerateAppId(/*manifest_id=*/std::nullopt, url);
    }

    // AndroidSmsAppSetupControllerImpl::PwaDelegate:
    std::optional<webapps::AppId> GetPwaForUrl(const GURL& install_url,
                                               Profile* profile) override {
      if (!base::Contains(url_to_pwa_map_, install_url))
        return std::nullopt;

      return url_to_pwa_map_[install_url];
    }

    network::mojom::CookieManager* GetCookieManager(Profile* profile) override {
      return fake_cookie_manager_;
    }

    void RemovePwa(
        const webapps::AppId& app_id,
        Profile* profile,
        AndroidSmsAppSetupController::SuccessCallback callback) override {
      for (const auto& url_pwa_pair : url_to_pwa_map_) {
        if (url_pwa_pair.second == app_id) {
          url_to_pwa_map_.erase(url_pwa_pair.first);
          std::move(callback).Run(true);
          return;
        }
      }

      std::move(callback).Run(false);
    }

   private:
    raw_ptr<FakeCookieManager> fake_cookie_manager_;
    base::flat_map<GURL, webapps::AppId> url_to_pwa_map_;
  };

  AndroidSmsAppSetupControllerImplTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  ~AndroidSmsAppSetupControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    host_content_settings_map_->ClearSettingsForOneType(
        ContentSettingsType::NOTIFICATIONS);
    fake_cookie_manager_ = std::make_unique<FakeCookieManager>();
    auto test_pwa_delegate =
        std::make_unique<TestPwaDelegate>(fake_cookie_manager_.get());
    test_pwa_delegate_ = test_pwa_delegate.get();

    provider_ = web_app::FakeWebAppProvider::Get(&profile_);
    web_app::test::AwaitStartWebAppProviderAndSubsystems(&profile_);

    fake_externally_managed_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const web_app::ExternalInstallOptions& install_options) {
              return web_app::ExternallyManagedAppManager::InstallResult(
                  install_result_code_);
            }));

    setup_controller_ = base::WrapUnique(new AndroidSmsAppSetupControllerImpl(
        &profile_, &fake_externally_managed_app_manager(),
        host_content_settings_map_));

    std::unique_ptr<AndroidSmsAppSetupControllerImpl::PwaDelegate>
        base_delegate(test_pwa_delegate.release());

    static_cast<AndroidSmsAppSetupControllerImpl*>(setup_controller_.get())
        ->SetPwaDelegateForTesting(std::move(base_delegate));
  }

  void CallSetUpAppWithRetries(const GURL& app_url,
                               const GURL& install_url,
                               size_t num_failure_tries,
                               bool expected_setup_result) {
    const auto& install_requests =
        fake_externally_managed_app_manager().install_requests();
    size_t num_install_requests_before_call = install_requests.size();

    base::RunLoop run_loop;
    base::HistogramTester histogram_tester;

    SetInstallResultCode(
        webapps::InstallResultCode::kGetWebAppInstallInfoFailed);

    setup_controller_->SetUpApp(
        app_url, install_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::OnSetUpAppResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    fake_cookie_manager_->InvokePendingSetCanonicalCookieCallback(
        "default_to_persist" /* expected_cookie_name */,
        "true" /* expected_cookie_value */,
        GURL("https://" + app_url.host()) /* expected_source_url */,
        false /* expected_modify_http_only */,
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
        true /* success */);

    fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
        app_url, "cros_migrated_to" /* expected_cookie_name */,
        true /* success */);

    base::RunLoop().RunUntilIdle();

    // Fast forward through remaining attempts.
    for (size_t retry_count = 0; retry_count < num_failure_tries - 1;
         retry_count++) {
      EXPECT_NE(ContentSetting::CONTENT_SETTING_ALLOW,
                GetNotificationSetting(app_url));
      EXPECT_EQ(num_install_requests_before_call + retry_count + 1u,
                install_requests.size());
      EXPECT_EQ(GetInstallOptionsForUrl(install_url), install_requests.back());

      task_environment_.FastForwardBy(
          AndroidSmsAppSetupControllerImpl::kInstallRetryDelay *
          (1 << retry_count));
    }

    // Send success code for last attempt.
    SetInstallResultCode(webapps::InstallResultCode::kSuccessNewInstall);
    task_environment_.FastForwardBy(
        AndroidSmsAppSetupControllerImpl::kInstallRetryDelay *
        (1 << (num_failure_tries - 1)));

    if (last_set_up_app_result_ && *last_set_up_app_result_) {
      histogram_tester.ExpectBucketCount(
          "AndroidSms.NumAttemptsForSuccessfulInstallation",
          num_failure_tries + 1, 1);
    }
    histogram_tester.ExpectBucketCount(
        "AndroidSms.EffectivePWAInstallationSuccess", expected_setup_result, 1);

    EXPECT_EQ(last_set_up_app_result_, expected_setup_result);
    last_set_up_app_result_.reset();
  }

  void CallSetUpApp(const GURL& app_url,
                    const GURL& install_url,
                    size_t num_expected_app_installs) {
    const auto& install_requests =
        fake_externally_managed_app_manager().install_requests();
    size_t num_install_requests_before_call = install_requests.size();

    base::RunLoop run_loop;
    base::HistogramTester histogram_tester;

    setup_controller_->SetUpApp(
        app_url, install_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::OnSetUpAppResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    fake_cookie_manager_->InvokePendingSetCanonicalCookieCallback(
        "default_to_persist" /* expected_cookie_name */,
        "true" /* expected_cookie_value */,
        GURL("https://" + app_url.host()) /* expected_source_url */,
        false /* expected_modify_http_only */,
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
        true /* success */);

    fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
        app_url, "cros_migrated_to" /* expected_cookie_name */,
        true /* success */);
    base::RunLoop().RunUntilIdle();

    // If the PWA was not already installed at the URL, SetUpApp() should
    // install it.
    if (!test_pwa_delegate_->GetPwaForUrl(install_url, &profile_)) {
      EXPECT_EQ(num_install_requests_before_call + 1u, install_requests.size());
      EXPECT_EQ(GetInstallOptionsForUrl(install_url), install_requests.back());

      EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
                GetNotificationSetting(app_url));
    }

    if (num_expected_app_installs) {
      histogram_tester.ExpectBucketCount(
          "AndroidSms.PWAInstallationResult",
          webapps::InstallResultCode::kSuccessNewInstall,
          num_expected_app_installs);
      histogram_tester.ExpectBucketCount(
          "AndroidSms.EffectivePWAInstallationSuccess", true, 1);
    }

    run_loop.Run();
    EXPECT_TRUE(*last_set_up_app_result_);
    last_set_up_app_result_.reset();
  }

  void CallDeleteRememberDeviceByDefaultCookie(const GURL& app_url) {
    base::RunLoop run_loop;

    setup_controller_->DeleteRememberDeviceByDefaultCookie(
        app_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::
                           OnDeleteRememberDeviceByDefaultCookieResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
        app_url, "default_to_persist" /* expected_cookie_name */,
        true /* success */);

    run_loop.Run();
    EXPECT_TRUE(*last_delete_cookie_result_);
    last_delete_cookie_result_.reset();
  }

  void CallRemoveApp(const GURL& app_url,
                     const GURL& install_url,
                     const GURL& migrated_to_app_url,
                     size_t num_expected_app_uninstalls) {
    base::RunLoop run_loop;
    base::HistogramTester histogram_tester;

    bool was_installed =
        test_pwa_delegate_->GetPwaForUrl(install_url, &profile_).has_value();
    setup_controller_->RemoveApp(
        app_url, install_url, migrated_to_app_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::OnRemoveAppResult,
                       base::Unretained(this), run_loop.QuitClosure()));
    base::RunLoop().RunUntilIdle();

    // If the PWA was already installed at the URL, RemoveApp() should uninstall
    // the it.
    if (was_installed) {
      EXPECT_FALSE(test_pwa_delegate()->GetPwaForUrl(install_url, &profile_));

      fake_cookie_manager_->InvokePendingSetCanonicalCookieCallback(
          "cros_migrated_to" /* expected_cookie_name */,
          migrated_to_app_url.GetContent() /* expected_cookie_value */,
          GURL("https://" + app_url.host()) /* expected_source_url */,
          false /* expected_modify_http_only */,
          net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
          true /* success */);

      fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
          app_url, "default_to_persist" /* expected_cookie_name */,
          true /* success */);
      base::RunLoop().RunUntilIdle();
    }

    if (num_expected_app_uninstalls) {
      histogram_tester.ExpectBucketCount("AndroidSms.PWAUninstallationResult",
                                         true, num_expected_app_uninstalls);
    }

    run_loop.Run();
    EXPECT_TRUE(*last_remove_app_result_);
    last_remove_app_result_.reset();
  }

  TestPwaDelegate* test_pwa_delegate() { return test_pwa_delegate_; }

  void SetInstallResultCode(webapps::InstallResultCode result_code) {
    install_result_code_ = result_code;
  }

  web_app::FakeExternallyManagedAppManager&
  fake_externally_managed_app_manager() {
    return static_cast<web_app::FakeExternallyManagedAppManager&>(
        provider_->externally_managed_app_manager());
  }

 private:
  ContentSetting GetNotificationSetting(const GURL& url) {
    return host_content_settings_map_->GetContentSetting(
        url, GURL() /* top_level_url */, ContentSettingsType::NOTIFICATIONS);
  }

  void OnSetUpAppResult(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_up_app_result_);
    last_set_up_app_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnDeleteRememberDeviceByDefaultCookieResult(
      base::OnceClosure quit_closure,
      bool success) {
    EXPECT_FALSE(last_delete_cookie_result_);
    last_delete_cookie_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnRemoveAppResult(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_remove_app_result_);
    last_remove_app_result_ = success;
    std::move(quit_closure).Run();
  }

  webapps::InstallResultCode install_result_code_ =
      webapps::InstallResultCode::kSuccessNewInstall;

  content::BrowserTaskEnvironment task_environment_;

  std::optional<bool> last_set_up_app_result_;
  std::optional<bool> last_delete_cookie_result_;
  std::optional<bool> last_remove_app_result_;

  raw_ptr<web_app::FakeWebAppProvider, DanglingUntriaged> provider_;

  TestingProfile profile_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<FakeCookieManager> fake_cookie_manager_;
  raw_ptr<TestPwaDelegate, DanglingUntriaged> test_pwa_delegate_;
  std::unique_ptr<AndroidSmsAppSetupController> setup_controller_;
};

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_NoPreviousApp) {
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_AppAlreadyInstalled) {
  // Start with a PWA already installed at the URL.
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1));
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               0u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_OtherPwaInstalled) {
  // Start with a PWA already installed at a different URL.
  test_pwa_delegate()->SetHasPwa(GURL(kTestUrl2));
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpAppThenDeleteCookie) {
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  CallDeleteRememberDeviceByDefaultCookie(GURL(kTestUrl1));
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_Retry) {
  // Setup should fail when all attempts fail.
  CallSetUpAppWithRetries(
      GURL(kTestUrl1), GURL(kTestInstallUrl1),
      AndroidSmsAppSetupControllerImpl::kMaxInstallRetryCount +
          1 /* num_failure_tries*/,
      false /* expected_setup_result */);

  // Setup should succeed when the last attempt succeeds.
  CallSetUpAppWithRetries(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                          AndroidSmsAppSetupControllerImpl::
                              kMaxInstallRetryCount /* num_failure_tries*/,
                          true /* expected_setup_result */);

  // Setup should succeed when only fewer than max attempts fail.
  CallSetUpAppWithRetries(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                          1 /* num_failure_tries*/,
                          true /* expected_setup_result */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpAppThenRemove) {
  // Install and remove.
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1));
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                1u /* num_expected_app_uninstalls */);

  // Repeat once more.
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1));
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                1u /* num_expected_app_uninstalls */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, RemoveApp_NoInstalledApp) {
  // Do not have an installed app before attempting to remove it.
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                0u /* num_expected_app_uninstalls */);
}

}  // namespace android_sms
}  // namespace ash

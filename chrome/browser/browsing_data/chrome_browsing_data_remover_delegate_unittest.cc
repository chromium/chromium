// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/monitor.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/mock_password_sync_metadata_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_isolation/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/customtabs/origin_verifier.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "components/feed/buildflags.h"
#else  // !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/common/chrome_paths.h"
#include "components/crash/core/app/crashpad.h"
#include "components/upload_list/crash_upload_list.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_utils.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

using content::BrowsingDataFilterBuilder;
using domain_reliability::DomainReliabilityClearMode;
using domain_reliability::DomainReliabilityMonitor;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::FloatEq;
using testing::Invoke;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
using testing::SizeIs;
using testing::WithArgs;

namespace {

const char kTestRegisterableDomain1[] = "host1.com";
const char kTestRegisterableDomain3[] = "host3.com";

// For HTTP auth.
const char kTestRealm[] = "TestRealm";

// For Autofill.
const char kWebOrigin[] = "https://www.example.com/";

// Shorthands for origin types.
#if BUILDFLAG(ENABLE_EXTENSIONS)
const uint64_t kExtension =
    ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION;
#endif
const uint64_t kProtected =
    content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
const uint64_t kUnprotected =
    content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL Origin1() {
  return GURL("http://host1.com:1");
}
GURL Origin2() {
  return GURL("http://host2.com:1");
}
GURL Origin3() {
  return GURL("http://host3.com:1");
}
GURL Origin4() {
  return GURL("https://host3.com:1");
}

GURL OriginExt() {
  return GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz");
}
GURL OriginDevTools() {
  return GURL("devtools://abcdefghijklmnopqrstuvw");
}

GURL DSEOrigin() {
  return GURL("https://search.com");
}

// Testers --------------------------------------------------------------------

#if defined(OS_ANDROID)
class TestWebappRegistry : public WebappRegistry {
 public:
  TestWebappRegistry() : WebappRegistry() {}

  void UnregisterWebappsForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }

  void ClearWebappHistoryForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }
};

// TestSearchEngineDelegate
class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  base::string16 GetDSEName() override { return base::string16(); }

  url::Origin GetDSEOrigin() override {
    return url::Origin::Create(DSEOrigin());
  }

  void SetDSEChangedCallback(base::RepeatingClosure callback) override {
    dse_changed_callback_ = std::move(callback);
  }

  void UpdateDSEOrigin() { dse_changed_callback_.Run(); }

 private:
  base::RepeatingClosure dse_changed_callback_;
};
#endif

#if defined(OS_CHROMEOS)
// Customized fake class to count TpmAttestationDeleteKeys call.
class FakeCryptohomeClient : public chromeos::FakeCryptohomeClient {
 public:
  void TpmAttestationDeleteKeysByPrefix(
      chromeos::attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      chromeos::DBusMethodCallback<bool> callback) override {
    ++delete_keys_call_count_;
    chromeos::FakeCryptohomeClient::TpmAttestationDeleteKeysByPrefix(
        key_type, cryptohome_id, key_prefix, std::move(callback));
  }

  int delete_keys_call_count() const { return delete_keys_call_count_; }

 private:
  int delete_keys_call_count_ = 0;
};
#endif

class RemoveCookieTester {
 public:
  RemoveCookieTester() {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    bool result = false;
    base::RunLoop run_loop;
    cookie_manager_->GetCookieList(
        Origin1(), net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting(
            [&](const net::CookieAccessResultList& cookie_list,
                const net::CookieAccessResultList& excluded_cookies) {
              std::string cookie_line =
                  net::CanonicalCookie::BuildCookieLine(cookie_list);
              if (cookie_line == "A=1") {
                result = true;
              } else {
                EXPECT_EQ("", cookie_line);
                result = false;
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  void AddCookie() {
    base::RunLoop run_loop;
    auto cookie = net::CanonicalCookie::Create(
        Origin1(), "A=1", base::Time::Now(), base::nullopt /* server_time */);
    cookie_manager_->SetCanonicalCookie(
        *cookie, Origin1(), net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting([&](net::CookieAccessResult result) {
          EXPECT_TRUE(result.status.IsInclude());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  void SetCookieManager(
      mojo::Remote<network::mojom::CookieManager> cookie_manager) {
    cookie_manager_ = std::move(cookie_manager);
  }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

class RemoveSafeBrowsingCookieTester : public RemoveCookieTester {
 public:
  RemoveSafeBrowsingCookieTester()
      : browser_process_(TestingBrowserProcess::GetGlobal()) {
    // TODO(crbug/925153): Port consumers of the |sb_service| to use the
    // interface in components/safe_browsing, and remove this cast.
    scoped_refptr<safe_browsing::SafeBrowsingService> sb_service =
        static_cast<safe_browsing::SafeBrowsingService*>(
            safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service.get());
    sb_service->Initialize();
    base::RunLoop().RunUntilIdle();

    // Make sure the safe browsing cookie store has no cookies.
    // TODO(mmenke): Is this really needed?
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    sb_service->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    cookie_manager->DeleteCookies(
        network::mojom::CookieDeletionFilter::New(),
        base::BindLambdaForTesting(
            [&](uint32_t num_deleted) { run_loop.Quit(); }));
    run_loop.Run();

    SetCookieManager(std::move(cookie_manager));
  }

  virtual ~RemoveSafeBrowsingCookieTester() {
    browser_process_->safe_browsing_service()->ShutDown();
    base::RunLoop().RunUntilIdle();
    browser_process_->SetSafeBrowsingService(nullptr);
  }

 private:
  TestingBrowserProcess* browser_process_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSafeBrowsingCookieTester);
};

class RemoveHistoryTester {
 public:
  RemoveHistoryTester() {}

  bool Init(Profile* profile) WARN_UNUSED_RESULT {
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_)
      return false;

    return true;
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    bool contains_url = false;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service_->QueryURL(
        url, true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          contains_url = result.success;
          run_loop.Quit();
        }),
        &tracker);
    run_loop.Run();

    return contains_url;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false, false);
  }

 private:
  // TestingProfile owns the history service; we shouldn't delete it.
  history::HistoryService* history_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveFaviconTester {
 public:
  RemoveFaviconTester() {}

  bool Init(Profile* profile) WARN_UNUSED_RESULT {
    // Create the history service if it has not been created yet.
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_)
      return false;

    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return false;

    return true;
  }

  // Returns true if there is a favicon stored for |page_url| in the favicon
  // database.
  bool HasFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_favicon_;
  }

  // Returns true if:
  // - There is a favicon stored for |page_url| in the favicon database.
  // - The stored favicon is expired.
  bool HasExpiredFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_expired_favicon_;
  }

  // Adds a visit to history and stores an arbitrary favicon bitmap for
  // |page_url|.
  void VisitAndAddFavicon(const GURL& page_url) {
    history_service_->AddPage(page_url, base::Time::Now(), nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false, false);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
    bitmap.eraseColor(SK_ColorBLUE);
    favicon_service_->SetFavicons({page_url}, page_url,
                                  favicon_base::IconType::kFavicon,
                                  gfx::Image::CreateFrom1xBitmap(bitmap));
  }

 private:
  // Synchronously requests the favicon for |page_url| from the favicon
  // database.
  void RequestFaviconSyncForPageURL(const GURL& page_url) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    favicon_service_->GetRawFaviconForPageURL(
        page_url, {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
        /*fallback_to_host=*/false,
        base::BindOnce(&RemoveFaviconTester::SaveResultAndQuit,
                       base::Unretained(this)),
        &tracker_);
    run_loop.Run();
  }

  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(const favicon_base::FaviconRawBitmapResult& result) {
    got_favicon_ = result.is_valid();
    got_expired_favicon_ = result.is_valid() && result.expired;
    quit_closure_.Run();
  }

  // For favicon requests.
  base::CancelableTaskTracker tracker_;
  bool got_favicon_ = false;
  bool got_expired_favicon_ = false;
  base::Closure quit_closure_;

  // Owned by TestingProfile.
  history::HistoryService* history_service_ = nullptr;
  favicon::FaviconService* favicon_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveFaviconTester);
};

std::unique_ptr<KeyedService> BuildProtocolHandlerRegistry(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ProtocolHandlerRegistry>(
      profile, std::make_unique<TestProtocolHandlerRegistryDelegate>());
}

class ClearDomainReliabilityTester {
 public:
  explicit ClearDomainReliabilityTester(TestingProfile* profile) {
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile->GetBrowsingDataRemoverDelegate())
        ->OverrideDomainReliabilityClearerForTesting(base::BindRepeating(
            &ClearDomainReliabilityTester::Clear, base::Unretained(this)));
  }

  unsigned clear_count() const { return clear_count_; }

  network::mojom::NetworkContext::DomainReliabilityClearMode last_clear_mode()
      const {
    return last_clear_mode_;
  }

  const base::RepeatingCallback<bool(const GURL&)>& last_filter() const {
    return last_filter_;
  }

 private:
  void Clear(
      content::BrowsingDataFilterBuilder* filter_builder,
      network::mojom::NetworkContext_DomainReliabilityClearMode mode,
      network::mojom::NetworkContext::ClearDomainReliabilityCallback callback) {
    ++clear_count_;
    last_clear_mode_ = mode;
    std::move(callback).Run();

    last_filter_ = filter_builder->MatchesAllOriginsAndDomains()
                       ? base::RepeatingCallback<bool(const GURL&)>()
                       : filter_builder->BuildUrlFilter();
  }

  unsigned clear_count_ = 0;
  network::mojom::NetworkContext::DomainReliabilityClearMode last_clear_mode_;
  base::RepeatingCallback<bool(const GURL&)> last_filter_;
};

class RemovePasswordsTester {
 public:
  explicit RemovePasswordsTester(TestingProfile* testing_profile) {
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        testing_profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                content::BrowserContext,
                testing::NiceMock<password_manager::MockPasswordStore>>));

    profile_store_ = static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetForProfile(testing_profile,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get());

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
          testing_profile,
          base::BindRepeating(
              &password_manager::BuildPasswordStore<
                  content::BrowserContext,
                  testing::NiceMock<password_manager::MockPasswordStore>>));

      account_store_ = static_cast<password_manager::MockPasswordStore*>(
          AccountPasswordStoreFactory::GetForProfile(
              testing_profile, ServiceAccessType::EXPLICIT_ACCESS)
              .get());
      ON_CALL(*account_store_, GetMetadataStore())
          .WillByDefault(Return(&account_metadata_store_));
    }

    OSCryptMocker::SetUp();
  }

  ~RemovePasswordsTester() { OSCryptMocker::TearDown(); }

  password_manager::MockPasswordStore* profile_store() {
    return profile_store_;
  }

  password_manager::MockPasswordStore* account_store() {
    return account_store_;
  }

  password_manager::MockPasswordSyncMetadataStore* account_metadata_store() {
    return &account_metadata_store_;
  }

 private:
  password_manager::MockPasswordStore* profile_store_;
  password_manager::MockPasswordStore* account_store_;
  password_manager::MockPasswordSyncMetadataStore account_metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(RemovePasswordsTester);
};

class RemovePermissionPromptCountsTest {
 public:
  explicit RemovePermissionPromptCountsTest(TestingProfile* profile)
      : autoblocker_(
            PermissionDecisionAutoBlockerFactory::GetForProfile(profile)) {}

  int GetDismissCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetDismissCount(url, permission);
  }

  int GetIgnoreCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetIgnoreCount(url, permission);
  }

  bool RecordIgnoreAndEmbargo(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->RecordIgnoreAndEmbargo(url, permission, false);
  }

  bool RecordDismissAndEmbargo(const GURL& url,
                               ContentSettingsType permission) {
    return autoblocker_->RecordDismissAndEmbargo(url, permission, false);
  }

  void CheckEmbargo(const GURL& url,
                    ContentSettingsType permission,
                    ContentSetting expected_setting) {
    EXPECT_EQ(expected_setting,
              autoblocker_->GetEmbargoResult(url, permission).content_setting);
  }

 private:
  permissions::PermissionDecisionAutoBlocker* autoblocker_;

  DISALLOW_COPY_AND_ASSIGN(RemovePermissionPromptCountsTest);
};

#if BUILDFLAG(ENABLE_PLUGINS)
// A small modification to MockBrowsingDataFlashLSOHelper so that it responds
// immediately and does not wait for the Notify() call. Otherwise it would
// deadlock BrowsingDataRemoverImpl::RemoveImpl.
class TestBrowsingDataFlashLSOHelper : public MockBrowsingDataFlashLSOHelper {
 public:
  explicit TestBrowsingDataFlashLSOHelper(TestingProfile* profile)
      : MockBrowsingDataFlashLSOHelper(profile) {}

  void StartFetching(GetSitesWithFlashDataCallback callback) override {
    MockBrowsingDataFlashLSOHelper::StartFetching(std::move(callback));
    Notify();
  }

 private:
  ~TestBrowsingDataFlashLSOHelper() override {}

  DISALLOW_COPY_AND_ASSIGN(TestBrowsingDataFlashLSOHelper);
};

class RemovePluginDataTester {
 public:
  explicit RemovePluginDataTester(TestingProfile* profile)
      : helper_(new TestBrowsingDataFlashLSOHelper(profile)) {
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile->GetBrowsingDataRemoverDelegate())
        ->OverrideFlashLSOHelperForTesting(helper_);
  }

  void AddDomain(const std::string& domain) {
    helper_->AddFlashLSODomain(domain);
  }

  const std::vector<std::string>& GetDomains() {
    // TestBrowsingDataFlashLSOHelper is synchronous, so we can immediately
    // return the fetched domains.
    helper_->StartFetching(
        base::BindOnce(&RemovePluginDataTester::OnSitesWithFlashDataFetched,
                       base::Unretained(this)));
    return domains_;
  }

 private:
  void OnSitesWithFlashDataFetched(const std::vector<std::string>& sites) {
    domains_ = sites;
  }

  std::vector<std::string> domains_;
  scoped_refptr<TestBrowsingDataFlashLSOHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(RemovePluginDataTester);
};

// Waits until a change is observed in content settings.
class FlashContentSettingsChangeWaiter : public content_settings::Observer {
 public:
  explicit FlashContentSettingsChangeWaiter(Profile* profile)
      : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }
  ~FlashContentSettingsChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier) override {
    if (content_type == ContentSettingsType::PLUGINS)
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(FlashContentSettingsChangeWaiter);
};
#endif

// Custom matcher to test the equivalence of two URL filters. Since those are
// blackbox predicates, we can only approximate the equivalence by testing
// whether the filter give the same answer for several URLs. This is currently
// good enough for our testing purposes, to distinguish filters that delete or
// preserve origins, empty and non-empty filters and such.
//
// TODO(msramek): BrowsingDataRemover and some of its backends support URL
// filters, but its constructor currently only takes a single URL and constructs
// its own url filter. If an url filter was directly passed to
// BrowsingDataRemover (what should eventually be the case), we can use the same
// instance in the test as well, and thus simply test
// base::RepeatingCallback::Equals() in this matcher.
class ProbablySameFilterMatcher
    : public MatcherInterface<
          const base::RepeatingCallback<bool(const GURL&)>&> {
 public:
  explicit ProbablySameFilterMatcher(
      const base::RepeatingCallback<bool(const GURL&)>& filter)
      : to_match_(filter) {}

  virtual bool MatchAndExplain(
      const base::RepeatingCallback<bool(const GURL&)>& filter,
      MatchResultListener* listener) const {
    if (filter.is_null() && to_match_.is_null())
      return true;
    if (filter.is_null() != to_match_.is_null())
      return false;

    const GURL urls_to_test_[] = {Origin1(), Origin2(), Origin3(),
                                  GURL("invalid spec")};
    for (GURL url : urls_to_test_) {
      if (filter.Run(url) != to_match_.Run(url)) {
        if (listener)
          *listener << "The filters differ on the URL " << url;
        return false;
      }
    }
    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "is probably the same url filter as " << &to_match_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "is definitely NOT the same url filter as " << &to_match_;
  }

 private:
  const base::RepeatingCallback<bool(const GURL&)>& to_match_;
};

inline Matcher<const base::RepeatingCallback<bool(const GURL&)>&>
ProbablySameFilter(const base::RepeatingCallback<bool(const GURL&)>& filter) {
  return MakeMatcher(new ProbablySameFilterMatcher(filter));
}

bool ProbablySameFilters(
    const base::RepeatingCallback<bool(const GURL&)>& filter1,
    const base::RepeatingCallback<bool(const GURL&)>& filter2) {
  return ProbablySameFilter(filter1).MatchAndExplain(filter2, nullptr);
}

base::Time AnHourAgo() {
  return base::Time::Now() - base::TimeDelta::FromHours(1);
}

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(TestingProfile* testing_profile)
      : download_manager_(new content::MockDownloadManager()) {
    content::BrowserContext::SetDownloadManagerForTesting(
        testing_profile, base::WrapUnique(download_manager_));
    std::unique_ptr<ChromeDownloadManagerDelegate> delegate =
        std::make_unique<ChromeDownloadManagerDelegate>(testing_profile);
    chrome_download_manager_delegate_ = delegate.get();
    service_ =
        DownloadCoreServiceFactory::GetForBrowserContext(testing_profile);
    service_->SetDownloadManagerDelegateForTesting(std::move(delegate));

    EXPECT_CALL(*download_manager_, GetBrowserContext())
        .WillRepeatedly(Return(testing_profile));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() {
    service_->SetDownloadManagerDelegateForTesting(nullptr);
  }

  content::MockDownloadManager* download_manager() { return download_manager_; }

 private:
  DownloadCoreService* service_;
  content::MockDownloadManager* download_manager_;  // Owned by testing profile.
  ChromeDownloadManagerDelegate* chrome_download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
};

}  // namespace

ACTION(QuitMainMessageLoop) {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

class PersonalDataLoadedObserverMock
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  ~PersonalDataLoadedObserverMock() override {}
  MOCK_METHOD0(OnPersonalDataChanged, void());
  MOCK_METHOD0(OnPersonalDataFinishedProfileTasks, void());
};

// RemoveAutofillTester is not a part of the anonymous namespace above, as
// PersonalDataManager declares it a friend in an empty namespace.
class RemoveAutofillTester {
 public:
  explicit RemoveAutofillTester(TestingProfile* profile)
      : personal_data_manager_(
            autofill::PersonalDataManagerFactory::GetForProfile(profile)) {
    autofill::test::DisableSystemServices(profile->GetPrefs());
    personal_data_manager_->AddObserver(&personal_data_observer_);
  }

  ~RemoveAutofillTester() {
    personal_data_manager_->RemoveObserver(&personal_data_observer_);
    autofill::test::ReenableSystemServices();
  }

  // Returns true if there are autofill profiles.
  bool HasProfile() {
    return !personal_data_manager_->GetProfiles().empty() &&
           !personal_data_manager_->GetCreditCards().empty();
  }

  bool HasOrigin(const std::string& origin) {
    const std::vector<autofill::AutofillProfile*>& profiles =
        personal_data_manager_->GetProfiles();
    for (const autofill::AutofillProfile* profile : profiles) {
      if (profile->origin() == origin)
        return true;
    }

    const std::vector<autofill::CreditCard*>& credit_cards =
        personal_data_manager_->GetCreditCards();
    for (const autofill::CreditCard* credit_card : credit_cards) {
      if (credit_card->origin() == origin)
        return true;
    }

    return false;
  }

  // Add two profiles and two credit cards to the database.  In each pair, one
  // entry has a web origin and the other has a Chrome origin.
  void AddProfilesAndCards() {
    std::vector<autofill::AutofillProfile> profiles;
    autofill::AutofillProfile profile;
    profile.set_guid(base::GenerateGUID());
    profile.set_origin(kWebOrigin);
    profile.SetRawInfo(autofill::NAME_FIRST, base::ASCIIToUTF16("Bob"));
    profile.SetRawInfo(autofill::NAME_LAST, base::ASCIIToUTF16("Smith"));
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, base::ASCIIToUTF16("94043"));
    profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                       base::ASCIIToUTF16("sue@example.com"));
    profile.SetRawInfo(autofill::COMPANY_NAME, base::ASCIIToUTF16("Company X"));
    profiles.push_back(profile);

    profile.set_guid(base::GenerateGUID());
    profile.set_origin(autofill::kSettingsOrigin);
    profiles.push_back(profile);

    personal_data_manager_->SetProfiles(&profiles);

    WaitForOnPersonalDataFinishedProfileTasks();

    std::vector<autofill::CreditCard> cards;
    autofill::CreditCard card;
    card.set_guid(base::GenerateGUID());
    card.set_origin(kWebOrigin);
    card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                    base::ASCIIToUTF16("1234-5678-9012-3456"));
    cards.push_back(card);

    card.set_guid(base::GenerateGUID());
    card.set_origin(autofill::kSettingsOrigin);
    cards.push_back(card);

    personal_data_manager_->SetCreditCards(&cards);
    WaitForOnPersonalDataChanged();
  }

 private:
  void WaitForOnPersonalDataChanged() {
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMainMessageLoop());
    base::RunLoop().Run();
  }

  void WaitForOnPersonalDataFinishedProfileTasks() {
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillRepeatedly(QuitMainMessageLoop());
    base::RunLoop().Run();
  }

  autofill::PersonalDataManager* personal_data_manager_;
  PersonalDataLoadedObserverMock personal_data_observer_;
  DISALLOW_COPY_AND_ASSIGN(RemoveAutofillTester);
};

#if BUILDFLAG(ENABLE_REPORTING)
class MockReportingService : public net::ReportingService {
 public:
  MockReportingService() = default;

  // net::ReportingService implementation:

  ~MockReportingService() override = default;

  void QueueReport(const GURL& url,
                   const std::string& user_agent,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body,
                   int depth) override {
    NOTREACHED();
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_value) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(uint64_t data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ++remove_calls_;
    last_data_type_mask_ = data_type_mask;
    last_origin_filter_ = origin_filter;
  }

  void RemoveAllBrowsingData(uint64_t data_type_mask) override {
    ++remove_all_calls_;
    last_data_type_mask_ = data_type_mask;
    last_origin_filter_ = base::RepeatingCallback<bool(const GURL&)>();
  }

  void OnShutdown() override {}

  const net::ReportingPolicy& GetPolicy() const override {
    static net::ReportingPolicy dummy_policy_;
    NOTREACHED();
    return dummy_policy_;
  }

  net::ReportingContext* GetContextForTesting() const override {
    NOTREACHED();
    return nullptr;
  }

  int remove_calls() const { return remove_calls_; }
  int remove_all_calls() const { return remove_all_calls_; }
  uint64_t last_data_type_mask() const { return last_data_type_mask_; }
  const base::RepeatingCallback<bool(const GURL&)>& last_origin_filter() const {
    return last_origin_filter_;
  }

 private:
  int remove_calls_ = 0;
  int remove_all_calls_ = 0;
  uint64_t last_data_type_mask_ = 0;
  base::RepeatingCallback<bool(const GURL&)> last_origin_filter_;

  DISALLOW_COPY_AND_ASSIGN(MockReportingService);
};

namespace autofill {

// StrikeDatabaseTester is in the autofill namespace since
// StrikeDatabase declares it as a friend in the autofill namespace.
class StrikeDatabaseTester {
 public:
  explicit StrikeDatabaseTester(Profile* profile)
      : strike_database_(
            autofill::StrikeDatabaseFactory::GetForProfile(profile)) {}

  bool IsEmpty() {
    int num_keys;
    base::RunLoop run_loop;
    strike_database_->LoadKeys(base::BindLambdaForTesting(
        [&](bool success, std::unique_ptr<std::vector<std::string>> keys) {
          num_keys = keys.get()->size();
          run_loop.Quit();
        }));
    run_loop.Run();
    return (num_keys == 0);
  }

 private:
  autofill::StrikeDatabase* strike_database_;
};

}  // namespace autofill

class ClearReportingCacheTester {
 public:
  ClearReportingCacheTester(network::NetworkContext* network_context,
                            bool create_service)
      : url_request_context_(network_context->url_request_context()) {
    if (create_service)
      service_ = std::make_unique<MockReportingService>();

    old_service_ = url_request_context_->reporting_service();
    url_request_context_->set_reporting_service(service_.get());
  }

  ~ClearReportingCacheTester() {
    DCHECK_EQ(service_.get(), url_request_context_->reporting_service());
    url_request_context_->set_reporting_service(old_service_);
  }

  const MockReportingService& mock() { return *service_; }

 private:
  net::URLRequestContext* url_request_context_;
  std::unique_ptr<MockReportingService> service_;
  net::ReportingService* old_service_;
};

class MockNetworkErrorLoggingService : public net::NetworkErrorLoggingService {
 public:
  MockNetworkErrorLoggingService() = default;

  // net::NetworkErrorLoggingService implementation:

  ~MockNetworkErrorLoggingService() override = default;

  void OnHeader(const url::Origin& origin,
                const net::IPAddress& received_ip_address,
                const std::string& value) override {
    NOTREACHED();
  }

  void OnRequest(RequestDetails details) override { NOTREACHED(); }

  void QueueSignedExchangeReport(SignedExchangeReportDetails details) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ++remove_calls_;
    last_origin_filter_ = origin_filter;
  }

  void RemoveAllBrowsingData() override {
    ++remove_all_calls_;
    last_origin_filter_ = base::RepeatingCallback<bool(const GURL&)>();
  }

  int remove_calls() const { return remove_calls_; }
  int remove_all_calls() const { return remove_all_calls_; }
  const base::RepeatingCallback<bool(const GURL&)>& last_origin_filter() const {
    return last_origin_filter_;
  }

 private:
  int remove_calls_ = 0;
  int remove_all_calls_ = 0;
  base::RepeatingCallback<bool(const GURL&)> last_origin_filter_;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkErrorLoggingService);
};

class ClearNetworkErrorLoggingTester {
 public:
  ClearNetworkErrorLoggingTester(network::NetworkContext* network_context,
                                 bool create_service)
      : url_request_context_(network_context->url_request_context()) {
    if (create_service)
      service_ = std::make_unique<MockNetworkErrorLoggingService>();

    url_request_context_->set_network_error_logging_service(service_.get());
  }

  ~ClearNetworkErrorLoggingTester() {
    DCHECK_EQ(service_.get(),
              url_request_context_->network_error_logging_service());
    url_request_context_->set_network_error_logging_service(nullptr);
  }

  const MockNetworkErrorLoggingService& mock() { return *service_; }

 private:
  net::URLRequestContext* url_request_context_;
  std::unique_ptr<MockNetworkErrorLoggingService> service_;

  DISALLOW_COPY_AND_ASSIGN(ClearNetworkErrorLoggingTester);
};
#endif  // BUILDFLAG(ENABLE_REPORTING)

// Test Class -----------------------------------------------------------------

class ChromeBrowsingDataRemoverDelegateTest : public testing::Test {
 public:
  ChromeBrowsingDataRemoverDelegateTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        StatefulSSLHostStateDelegateFactory::GetInstance(),
        StatefulSSLHostStateDelegateFactory::GetDefaultFactoryForTesting());
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        FaviconServiceFactory::GetInstance(),
        FaviconServiceFactory::GetDefaultFactory());

    profile_ = profile_builder.Build();

    remover_ = content::BrowserContext::GetBrowsingDataRemover(profile_.get());

    // Make sure the Network Service is started before making a NetworkContext.
    content::GetNetworkService();
    task_environment_.RunUntilIdle();

    auto network_context_params = network::mojom::NetworkContextParams::New();
    network_context_params->cert_verifier_params =
        content::GetCertVerifierParams(
            network::mojom::CertVerifierCreationParams::New());
    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    network_context_ = std::make_unique<network::NetworkContext>(
        network::NetworkService::GetNetworkServiceForTesting(),
        network_context_remote.InitWithNewPipeAndPassReceiver(),
        std::move(network_context_params));
    content::BrowserContext::GetDefaultStoragePartition(profile_.get())
        ->SetNetworkContextForTesting(std::move(network_context_remote));

    ProtocolHandlerRegistryFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildProtocolHandlerRegistry));

#if defined(OS_ANDROID)
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile_->GetBrowsingDataRemoverDelegate())
        ->OverrideWebappRegistryForTesting(
            base::WrapUnique<WebappRegistry>(new TestWebappRegistry()));

    SearchPermissionsService* service =
        SearchPermissionsService::Factory::GetForBrowserContext(profile_.get());
    std::unique_ptr<TestSearchEngineDelegate> delegate =
        std::make_unique<TestSearchEngineDelegate>();
    TestSearchEngineDelegate* delegate_ptr = delegate.get();
    service->SetSearchEngineDelegateForTest(std::move(delegate));
    delegate_ptr->UpdateDSEOrigin();
#endif
  }

  void TearDown() override {
    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    base::RunLoop().RunUntilIdle();

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  ~ChromeBrowsingDataRemoverDelegateTest() override = default;

  // Returns the set of data types for which the deletion failed.
  uint64_t BlockUntilBrowsingDataRemoved(const base::Time& delete_begin,
                                         const base::Time& delete_end,
                                         uint64_t remove_mask,
                                         bool include_protected_origins) {
    uint64_t origin_type_mask =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    if (include_protected_origins) {
      origin_type_mask |=
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }

    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveAndReply(delete_begin, delete_end, remove_mask,
                             origin_type_mask, &completion_observer);
    base::ThreadPoolInstance::Get()->FlushForTesting();
    completion_observer.BlockUntilCompletion();
    return completion_observer.failed_data_types();
  }

  void BlockUntilOriginDataRemoved(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveWithFilterAndReply(
        delete_begin, delete_end, remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    base::ThreadPoolInstance::Get()->FlushForTesting();
    completion_observer.BlockUntilCompletion();
  }

  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTimeForTesting();
  }

  uint64_t GetRemovalMask() {
    return remover_->GetLastUsedRemovalMaskForTesting();
  }

  uint64_t GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMaskForTesting();
  }

  network::NetworkContext* network_context() { return network_context_.get(); }

  TestingProfile* GetProfile() { return profile_.get(); }

  bool Match(const GURL& origin,
             uint64_t mask,
             storage::SpecialStoragePolicy* policy) {
    return remover_->DoesOriginMatchMaskForTesting(
        mask, url::Origin::Create(origin), policy);
  }

 private:
  // Cached pointer to BrowsingDataRemover for access to testing methods.
  content::BrowsingDataRemover* remover_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkContext> network_context_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowsingDataRemoverDelegateTest);
};

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  // Removing with time period other than all time should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieForeverWithPredicate) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.ContainsCookie());

  std::unique_ptr<BrowsingDataFilterBuilder> filter2(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter2->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter2));
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForever) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  tester.AddHistory(Origin1(), base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(Origin1()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(Origin1()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(Origin1(), base::Time::Now());
  tester.AddHistory(Origin2(), two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(Origin1()));
  ASSERT_TRUE(tester.HistoryContainsURL(Origin2()));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(Origin1()));
  EXPECT_TRUE(tester.HistoryContainsURL(Origin2()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForOlderThan30Days) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time older_than_29days =
      base::Time::Now() - base::TimeDelta::FromDays(29);
  base::Time older_than_30days =
      base::Time::Now() - base::TimeDelta::FromDays(30);
  base::Time older_than_31days =
      base::Time::Now() - base::TimeDelta::FromDays(31);

  tester.AddHistory(Origin1(), base::Time::Now());
  tester.AddHistory(Origin2(), older_than_29days);
  tester.AddHistory(Origin3(), older_than_31days);

  ASSERT_TRUE(tester.HistoryContainsURL(Origin1()));
  ASSERT_TRUE(tester.HistoryContainsURL(Origin2()));
  ASSERT_TRUE(tester.HistoryContainsURL(Origin3()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), older_than_30days,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  EXPECT_TRUE(tester.HistoryContainsURL(Origin1()));
  EXPECT_TRUE(tester.HistoryContainsURL(Origin2()));
  EXPECT_FALSE(tester.HistoryContainsURL(Origin3()));
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
// TODO(msramek): To make this testable, the refusal to delete history should
// be made a part of interface (e.g. a success value) as opposed to a DCHECK.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryProhibited) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(Origin1(), base::Time::Now());
  tester.AddHistory(Origin2(), two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(Origin1()));
  ASSERT_TRUE(tester.HistoryContainsURL(Origin2()));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(Origin1()));
  EXPECT_TRUE(tester.HistoryContainsURL(Origin2()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveMultipleTypesHistoryProhibited) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(Origin1(), base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(Origin1()));

  // Expect that passwords will be deleted, as they do not depend
  // on |prefs::kAllowDeletingBrowserHistory|.
  RemovePasswordsTester tester(GetProfile());
  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTimeImpl(_, _, _));

  uint64_t removal_mask =
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(), removal_mask,
                                false);
  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that history was not deleted.
  EXPECT_TRUE(history_tester.HistoryContainsURL(Origin1()));
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveExternalProtocolData) {
  TestingProfile* profile = GetProfile();
  url::Origin test_origin = url::Origin::Create(GURL("https://example.test"));
  const std::string serialized_test_origin = test_origin.Serialize();
  // Add external protocol data on profile.
  base::DictionaryValue prefs;
  prefs.SetKey(serialized_test_origin,
               base::Value(base::Value::Type::DICTIONARY));
  base::Value* allowed_protocols_for_origin =
      prefs.FindDictKey(serialized_test_origin);
  allowed_protocols_for_origin->SetBoolKey("tel", true);
  profile->GetPrefs()->Set(prefs::kProtocolHandlerPerOriginAllowedProtocols,
                           prefs);

  EXPECT_FALSE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kProtocolHandlerPerOriginAllowedProtocols)
          ->empty());

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_EXTERNAL_PROTOCOL_DATA,
      false);
  EXPECT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kProtocolHandlerPerOriginAllowedProtocols)
          ->empty());
}

// Check that clearing browsing data (either history or cookies with other site
// data) clears any saved isolated origins.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePersistentIsolatedOrigins) {
  PrefService* prefs = GetProfile()->GetPrefs();

  // Add foo.com to the list of stored isolated origins.
  base::ListValue list;
  list.AppendString("http://foo.com");
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Clear history and ensure the stored isolated origins are cleared.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Re-add foo.com to stored isolated origins.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Now clear cookies and other site data, and ensure foo.com is cleared.
  // Note that this uses a short time period to document that time ranges are
  // currently ignored by stored isolated origins.
  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Re-add foo.com.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Clear the isolated origins data type.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ISOLATED_ORIGINS, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Re-add foo.com.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());

  // Clear both history and site data, and ensure the stored isolated origins
  // are cleared.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
      false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->empty());
}

// Test that clearing history deletes favicons not associated with bookmarks.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveFaviconsForever) {
  GURL page_url("http://a");

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(page_url);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(page_url));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_FALSE(favicon_tester.HasFaviconForPageURL(page_url));
}

// Test that a bookmark's favicon is expired and not deleted when clearing
// history. Expiring the favicon causes the bookmark's favicon to be updated
// when the user next visits the bookmarked page. Expiring the bookmark's
// favicon is useful when the bookmark's favicon becomes incorrect (See
// crbug.com/474421 for a sample bug which causes this).
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ExpireBookmarkFavicons) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         base::ASCIIToUTF16("a"), bookmarked_page);

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(bookmarked_page);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(bookmarked_page));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_TRUE(favicon_tester.HasExpiredFaviconForPageURL(bookmarked_page));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DeleteBookmarks) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         base::ASCIIToUTF16("a"), bookmarked_page);
  EXPECT_EQ(1u, bookmark_model->bookmark_bar_node()->children().size());
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_BOOKMARKS, false);
  EXPECT_EQ(0u, bookmark_model->bookmark_bar_node()->children().size());
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_TimeBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(Origin1(), base::Time::Now());
  tester.AddHistory(Origin2(), two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(Origin1()));
  ASSERT_TRUE(tester.HistoryContainsURL(Origin2()));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, std::move(builder));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(Origin1()));
  EXPECT_TRUE(tester.HistoryContainsURL(Origin2()));
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  ProfileSyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify the clearing of autofill profiles added / modified more than 30 days
// ago.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalOlderThan30Days) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  ProfileSyncServiceFactory::GetForProfile(GetProfile());

  const base::Time kNow = base::Time::Now();
  const base::Time k30DaysOld = kNow - base::TimeDelta::FromDays(30);
  const base::Time k31DaysOld = kNow - base::TimeDelta::FromDays(31);
  const base::Time k32DaysOld = kNow - base::TimeDelta::FromDays(32);

  // Add profiles and cards with modification date as 31 days old from now.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(k31DaysOld);

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), k32DaysOld,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      k30DaysOld, base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), k30DaysOld,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  ProfileSyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       StrikeDatabaseEmptyOnAutofillRemoveEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  ProfileSyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  autofill::StrikeDatabaseTester strike_database_tester(GetProfile());
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);

  // StrikeDatabase should be empty when DATA_TYPE_FORM_DATA browsing data
  // gets deleted.
  ASSERT_TRUE(strike_database_tester.IsEmpty());
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       AutofillOriginsRemovedWithHistory) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  ProfileSyncServiceFactory::GetForProfile(GetProfile());

  tester.AddProfilesAndCards();
  EXPECT_FALSE(tester.HasOrigin(std::string()));
  EXPECT_TRUE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.HasOrigin(std::string()));
  EXPECT_FALSE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ZeroSuggestCacheClear) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults,
                   "[\"\", [\"foo\", \"bar\"]]");
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  // Expect the prefs to be cleared when cookies are removed.
  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
}

#if defined(OS_CHROMEOS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ContentProtectionPlatformKeysRemoval) {
  chromeos::MockUserManager* mock_user_manager =
      new testing::NiceMock<chromeos::MockUserManager>();
  mock_user_manager->SetActiveUser(
      AccountId::FromUserEmail("test@example.com"));
  user_manager::ScopedUserManager user_manager_enabler(
      base::WrapUnique(mock_user_manager));

  // Creates a derived fake global instance destroyed in
  // CryptohomeClient::Shutdown().
  auto* cryptohome_client = new FakeCryptohomeClient();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES, false);

  // Expect exactly one call.  No calls means no attempt to delete keys and more
  // than one call means a significant performance problem.
  EXPECT_EQ(1, cryptohome_client->delete_keys_call_count());

  chromeos::CryptohomeClient::Shutdown();
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Null) {
  ClearDomainReliabilityTester tester(GetProfile());

  EXPECT_EQ(0u, tester.clear_count());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Beacons) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(
      network::mojom::NetworkContext::DomainReliabilityClearMode::CLEAR_BEACONS,
      tester.last_clear_mode());
  EXPECT_TRUE(tester.last_filter().is_null());
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_Beacons_WithFilter) {
  ClearDomainReliabilityTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(
      network::mojom::NetworkContext::DomainReliabilityClearMode::CLEAR_BEACONS,
      tester.last_clear_mode());
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildUrlFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Contexts) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
  EXPECT_TRUE(tester.last_filter().is_null());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_Contexts_WithFilter) {
  ClearDomainReliabilityTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildUrlFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_ContextsWin) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_ProtectedOrigins) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                true);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
}

// TODO(juliatuttle): This isn't actually testing the no-monitor case, since
// BrowsingDataRemoverTest now creates one unconditionally, since it's needed
// for some unrelated test cases. This should be fixed so it tests the no-
// monitor case again.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_NoMonitor) {
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
}

// Tests that the deletion of downloads completes successfully and that
// ChromeDownloadManagerDelegate is correctly created and shut down.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDownloads) {
  RemoveDownloadsTester tester(GetProfile());
  EXPECT_CALL(*tester.download_manager(), RemoveDownloadsByURLAndTime(_, _, _));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordStatistics) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter;

  EXPECT_CALL(*tester.profile_store(), RemoveStatisticsByOriginAndTimeImpl(
                                           ProbablySameFilter(empty_filter),
                                           base::Time(), base::Time::Max()));
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordStatisticsByOrigin) {
  RemovePasswordsTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  EXPECT_CALL(*tester.profile_store(),
              RemoveStatisticsByOriginAndTimeImpl(
                  ProbablySameFilter(filter), base::Time(), base::Time::Max()));
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordsByTimeOnly) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.profile_store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemovePasswordsByTimeOnly_WithAccountStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  // Only DATA_TYPE_PASSWORDS is cleared. Accounts passwords are not affected.
  EXPECT_CALL(*tester.account_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .Times(0);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveAccountPasswordsByTimeOnly_WithAccountStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .Times(0);

  EXPECT_CALL(*tester.account_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  // For the account store, the remover delegate also waits until all the
  // deletions have propagated to the Sync server. Pretend that happens
  // immediately.
  EXPECT_CALL(*tester.account_metadata_store(), HasUnsyncedDeletions())
      .WillRepeatedly(Return(false));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ACCOUNT_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveAccountPasswordsByTimeOnly_WithAccountStore_Failure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .Times(0);

  EXPECT_CALL(*tester.account_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  // For the account store, the remover delegate also waits until all the
  // deletions have propagated to the Sync server. In this test, that never
  // happens.
  EXPECT_CALL(*tester.account_metadata_store(), HasUnsyncedDeletions())
      .WillRepeatedly(Return(true));
  // Bypass the (usually 30-second) timeout until the PasswordStore reports
  // failure.
  tester.account_store()->SetSyncTaskTimeoutForTest(base::TimeDelta());

  uint64_t failed_data_types = BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ACCOUNT_PASSWORDS, false);
  EXPECT_EQ(failed_data_types,
            ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ACCOUNT_PASSWORDS);
}

// Disabled, since passwords are not yet marked as a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordsByOrigin) {
  RemovePasswordsTester tester(GetProfile());
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  EXPECT_CALL(*tester.profile_store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS,
      std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DisableAutoSignIn) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.profile_store(),
              DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DisableAutoSignInAfterRemovingPasswords) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  EXPECT_CALL(*tester.profile_store(),
              DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS,
      false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveCompromisedCredentialsByTimeOnly) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter;

  EXPECT_CALL(
      *tester.profile_store(),
      RemoveCompromisedCredentialsByUrlAndTimeImpl(
          ProbablySameFilter(empty_filter), base::Time(), base::Time::Max()));
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveCompromisedAccountCredentialsByTimeOnly_WithAccountStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter;

  EXPECT_CALL(*tester.profile_store(),
              RemoveCompromisedCredentialsByUrlAndTimeImpl(_, _, _))
      .Times(0);

  EXPECT_CALL(
      *tester.account_store(),
      RemoveCompromisedCredentialsByUrlAndTimeImpl(
          ProbablySameFilter(empty_filter), base::Time(), base::Time::Max()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ACCOUNT_PASSWORDS, false);
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveCompromisedCredentialsByUrlAndTime) {
  RemovePasswordsTester tester(GetProfile());
  auto builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  EXPECT_CALL(*tester.profile_store(),
              RemoveCompromisedCredentialsByUrlAndTimeImpl(
                  ProbablySameFilter(filter), base::Time(), base::Time::Max()));
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveContentSettingsWithPreserveFilter) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin1(), GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      std::make_unique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin2(), GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      std::make_unique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin3(), GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      std::make_unique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin4(), GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      std::make_unique<base::DictionaryValue>());

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
      std::move(filter));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have true, and they're origin1, origin3, and origin4.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(3u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin1()),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin4()),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin3()),
            host_settings[2].primary_pattern)
      << host_settings[2].primary_pattern.ToString();
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveContentSettings) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingDefaultScope(Origin1(), Origin1(),
                                     ContentSettingsType::GEOLOCATION,
                                     std::string(), CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(DSEOrigin(), DSEOrigin(),
                                     ContentSettingsType::GEOLOCATION,
                                     std::string(), CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(Origin2(), Origin2(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(Origin3(), GURL(),
                                     ContentSettingsType::COOKIES,
                                     std::string(), CONTENT_SETTING_BLOCK);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  map->SetContentSettingCustomScope(pattern, ContentSettingsPattern::Wildcard(),
                                    ContentSettingsType::COOKIES, std::string(),
                                    CONTENT_SETTING_BLOCK);
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);

  // Everything except the default settings should be deleted. On Android the
  // default search engine setting should also not be deleted.
  bool expect_geolocation_dse_origin = false;
  bool expect_notifications_dse_origin = false;

#if defined(OS_ANDROID)
  expect_geolocation_dse_origin = true;
  expect_notifications_dse_origin = true;
#endif

  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, std::string(),
                             &host_settings);

  if (expect_geolocation_dse_origin) {
    ASSERT_EQ(2u, host_settings.size());
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(DSEOrigin()),
              host_settings[0].primary_pattern)
        << host_settings[0].primary_pattern.ToString();
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings[0].secondary_pattern)
        << host_settings[0].secondary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());

    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings[1].primary_pattern)
        << host_settings[1].primary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[1].GetContentSetting());
  } else {
    ASSERT_EQ(1u, host_settings.size());
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings[0].primary_pattern)
        << host_settings[0].primary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());
  }

  map->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS, std::string(),
                             &host_settings);

  if (expect_notifications_dse_origin) {
    ASSERT_EQ(2u, host_settings.size());
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(DSEOrigin()),
              host_settings[0].primary_pattern)
        << host_settings[0].primary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());

    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings[1].primary_pattern)
        << host_settings[1].primary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[1].GetContentSetting());
  } else {
    ASSERT_EQ(1u, host_settings.size());
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings[0].primary_pattern)
        << host_settings[0].primary_pattern.ToString();
    EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());
  }

  map->GetSettingsForOneType(ContentSettingsType::COOKIES, std::string(),
                             &host_settings);
  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveProtocolHandler) {
  auto* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile());
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time yesterday = base::Time::Now() - base::TimeDelta::FromDays(1);
  registry->OnAcceptRegisterProtocolHandler(
      ProtocolHandler::CreateProtocolHandler("news", Origin1()));
  registry->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("mailto", Origin1(), yesterday));
  EXPECT_TRUE(registry->IsHandledProtocol("news"));
  EXPECT_TRUE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      2U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
  // Delete last hour.
  BlockUntilBrowsingDataRemoved(
      one_hour_ago, base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(registry->IsHandledProtocol("news"));
  EXPECT_TRUE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      1U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
  // Delete everything.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(registry->IsHandledProtocol("news"));
  EXPECT_FALSE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      0U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveSelectedClientHints) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->AppendInteger(0);
  expiration_times_list->AppendInteger(2);

  double expiration_time =
      (base::Time::Now() + base::TimeDelta::FromHours(24)).ToDoubleT();

  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin1(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin2(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin3(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &host_settings);

  ASSERT_EQ(2u, host_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin1()),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin3()),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();

  for (size_t i = 0; i < host_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              host_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary, host_settings.at(i).setting_value);
  }
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveAllClientHints) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->AppendInteger(0);
  expiration_times_list->AppendInteger(2);

  double expiration_time =
      (base::Time::Now() + base::TimeDelta::FromHours(24)).ToDoubleT();

  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin1(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin2(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      Origin3(), GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      expiration_times_dictionary->CreateDeepCopy());

  // Clear all.
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &host_settings);

  ASSERT_EQ(0u, host_settings.size());
}

#if !defined(OS_ANDROID)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveZoomLevel) {
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(GetProfile());
  EXPECT_EQ(0u, zoom_map->GetAllZoomLevels().size());

  base::SimpleTestClock test_clock;
  zoom_map->SetClockForTesting(&test_clock);

  base::Time now = base::Time::Now();
  zoom_map->InitializeZoomLevelForHost(kTestRegisterableDomain1, 1.5,
                                       now - base::TimeDelta::FromHours(5));
  test_clock.SetNow(now - base::TimeDelta::FromHours(2));
  zoom_map->SetZoomLevelForHost(kTestRegisterableDomain3, 2.0);
  EXPECT_EQ(2u, zoom_map->GetAllZoomLevels().size());

  // Remove everything created during the last hour.
  BlockUntilBrowsingDataRemoved(
      now - base::TimeDelta::FromHours(1), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);

  // Nothing should be deleted as the zoomlevels were created earlier.
  EXPECT_EQ(2u, zoom_map->GetAllZoomLevels().size());

  test_clock.SetNow(now);
  zoom_map->SetZoomLevelForHost(kTestRegisterableDomain3, 2.0);

  // Remove everything changed during the last hour (domain3).
  BlockUntilBrowsingDataRemoved(
      now - base::TimeDelta::FromHours(1), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);

  // Verify we still have the zoom_level for domain1.
  auto levels = zoom_map->GetAllZoomLevels();
  EXPECT_EQ(1u, levels.size());
  EXPECT_EQ(kTestRegisterableDomain1, levels[0].host);

  zoom_map->SetZoomLevelForHostAndScheme("chrome", "print", 4.0);
  // Remove everything.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);

  // Host and scheme zoomlevels should not be affected.
  levels = zoom_map->GetAllZoomLevels();
  EXPECT_EQ(1u, levels.size());
  EXPECT_EQ("chrome", levels[0].scheme);
  EXPECT_EQ("print", levels[0].host);
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveTranslateBlocklist) {
  auto translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetProfile()->GetPrefs());
  translate_prefs->BlacklistSite("google.com");
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  base::Time t = base::Time::Now();
  translate_prefs->BlacklistSite("maps.google.com");

  EXPECT_TRUE(translate_prefs->IsSiteBlacklisted("google.com"));
  EXPECT_TRUE(translate_prefs->IsSiteBlacklisted("maps.google.com"));

  BlockUntilBrowsingDataRemoved(
      t, base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_TRUE(translate_prefs->IsSiteBlacklisted("google.com"));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted("maps.google.com"));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted("google.com"));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted("maps.google.com"));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDurablePermission) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(Origin1(), GURL(),
                                          CONTENT_SETTING_ALLOW);
  durable_permission.UpdateContentSetting(Origin2(), GURL(),
                                          CONTENT_SETTING_ALLOW);

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_DURABLE_PERMISSION,
      std::move(filter));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_DURABLE_PERMISSION,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have allow for the first origin.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, std::string(), &host_settings);

  ASSERT_EQ(2u, host_settings.size());
  // Only the first should should have a setting.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(Origin1()),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());

  // And our wildcard.
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[1].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DurablePermissionIsPartOfEmbedderDOMStorage) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(Origin1(), GURL(),
                                          CONTENT_SETTING_ALLOW);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, std::string(), &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE, false);

  // After the deletion, only the wildcard should remain.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, std::string(), &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
}

// Test that removing passwords clears HTTP auth data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ClearHttpAuthCache_RemovePasswords) {
  net::HttpNetworkSession* http_session = network_context()
                                              ->url_request_context()
                                              ->http_transaction_factory()
                                              ->GetSession();
  DCHECK(http_session);

  net::HttpAuthCache* http_auth_cache = http_session->http_auth_cache();
  http_auth_cache->Add(Origin1(), net::HttpAuth::AUTH_SERVER, kTestRealm,
                       net::HttpAuth::AUTH_SCHEME_BASIC,
                       net::NetworkIsolationKey(), "test challenge",
                       net::AuthCredentials(base::ASCIIToUTF16("foo"),
                                            base::ASCIIToUTF16("bar")),
                       "/");
  CHECK(http_auth_cache->Lookup(Origin1(), net::HttpAuth::AUTH_SERVER,
                                kTestRealm, net::HttpAuth::AUTH_SCHEME_BASIC,
                                net::NetworkIsolationKey()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);

  EXPECT_EQ(nullptr,
            http_auth_cache->Lookup(
                Origin1(), net::HttpAuth::AUTH_SERVER, kTestRealm,
                net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ClearPermissionPromptCounts) {
  RemovePermissionPromptCountsTest tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_1(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter_builder_1->AddRegisterableDomain(kTestRegisterableDomain1);

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_2(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter_builder_2->AddRegisterableDomain(kTestRegisterableDomain1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {permissions::features::kBlockPromptsIfDismissedOften}, {});

  {
    // Test REMOVE_HISTORY.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin2(), ContentSettingsType::DURABLE_STORAGE));
    tester.CheckEmbargo(Origin2(), ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin2(), ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin2(), ContentSettingsType::NOTIFICATIONS));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        Origin2(), ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(Origin2(), ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilOriginDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
        std::move(filter_builder_1));

    // Origin1() should be gone, but Origin2() remains.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin1(),
                                       ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(1, tester.GetIgnoreCount(Origin2(),
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(3, tester.GetDismissCount(Origin2(),
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(Origin2(), ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

    // Everything should be gone.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin1(),
                                       ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin2(),
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(Origin2(),
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(Origin2(), ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
  }
  {
    // Test REMOVE_SITE_DATA.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin1(), ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin1(), ContentSettingsType::MIDI_SYSEX));
    tester.CheckEmbargo(Origin1(), ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        Origin2(), ContentSettingsType::DURABLE_STORAGE));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin2(), ContentSettingsType::NOTIFICATIONS));

    BlockUntilOriginDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
        std::move(filter_builder_2));

    // Origin2() should be gone, but Origin1() remains.
    EXPECT_EQ(
        2, tester.GetIgnoreCount(Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(1, tester.GetIgnoreCount(Origin1(),
                                       ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        1, tester.GetDismissCount(Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin2(),
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(Origin2(),
                                        ContentSettingsType::NOTIFICATIONS));

    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(
        3, tester.GetDismissCount(Origin1(), ContentSettingsType::MIDI_SYSEX));
    tester.CheckEmbargo(Origin1(), ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA, false);

    // Everything should be gone.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(Origin1(), ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin1(),
                                       ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(Origin1(), ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(Origin2(),
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(Origin2(),
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(Origin1(), ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Check the |ContentSettingsType::PLUGINS_DATA| content setting is cleared
// with browsing data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ClearFlashPreviouslyChanged) {
  ChromePluginServiceFilter::GetInstance()->RegisterProfile(GetProfile());

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  // PLUGINS_DATA gets cleared with history OR site usage data.
  for (ChromeBrowsingDataRemoverDelegate::DataType data_type :
       {ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY}) {
    FlashContentSettingsChangeWaiter waiter(GetProfile());
    host_content_settings_map->SetContentSettingDefaultScope(
        Origin1(), Origin1(), ContentSettingsType::PLUGINS, std::string(),
        CONTENT_SETTING_ALLOW);
    host_content_settings_map->SetContentSettingDefaultScope(
        Origin2(), Origin2(), ContentSettingsType::PLUGINS, std::string(),
        CONTENT_SETTING_BLOCK);
    waiter.Wait();

    // Check that as a result, the PLUGINS_DATA prefs were populated.
    EXPECT_NE(nullptr,
              host_content_settings_map->GetWebsiteSetting(
                  Origin1(), Origin1(), ContentSettingsType::PLUGINS_DATA,
                  std::string(), nullptr));
    EXPECT_NE(nullptr,
              host_content_settings_map->GetWebsiteSetting(
                  Origin2(), Origin2(), ContentSettingsType::PLUGINS_DATA,
                  std::string(), nullptr));

    std::unique_ptr<BrowsingDataFilterBuilder> filter(
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kPreserve));
    BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(), data_type,
                                std::move(filter));
    EXPECT_EQ(nullptr,
              host_content_settings_map->GetWebsiteSetting(
                  Origin1(), Origin1(), ContentSettingsType::PLUGINS_DATA,
                  std::string(), nullptr));
    EXPECT_EQ(nullptr,
              host_content_settings_map->GetWebsiteSetting(
                  Origin2(), Origin2(), ContentSettingsType::PLUGINS_DATA,
                  std::string(), nullptr));
  }
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePluginData) {
  RemovePluginDataTester tester(GetProfile());

  tester.AddDomain(Origin1().host());
  tester.AddDomain(Origin2().host());
  tester.AddDomain(Origin3().host());

  std::vector<std::string> expected = {Origin1().host(), Origin2().host(),
                                       Origin3().host()};
  EXPECT_EQ(expected, tester.GetDomains());

  // Delete data with a filter for the registrable domain of |Origin3()|.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter_builder->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA,
      std::move(filter_builder));

  // Plugin data for |Origin3().host()| should have been removed.
  expected.pop_back();
  EXPECT_EQ(expected, tester.GetDomains());

  // TODO(msramek): Mock PluginDataRemover and test the complete deletion
  // of plugin data as well.
}
#endif

// Test that the remover clears language model data (normally added by the
// LanguageDetectionDriver).
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       LanguageHistogramClearedOnClearingCompleteHistory) {
  language::UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetForBrowserContext(GetProfile());

  // Simulate browsing.
  for (int i = 0; i < 100; i++) {
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("es");
  }

  // Clearing a part of the history has no effect.
  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(2));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.75));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.25));

  // Clearing the full history does the trick.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.0));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasks) {
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  // Protect Origin1().
  mock_policy->AddProtected(Origin1().GetOrigin());

  EXPECT_FALSE(Match(Origin1(), kUnprotected, mock_policy.get()));
  EXPECT_TRUE(Match(Origin2(), kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(OriginExt(), kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(OriginDevTools(), kUnprotected, mock_policy.get()));

  EXPECT_TRUE(Match(Origin1(), kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(Origin2(), kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(OriginExt(), kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(OriginDevTools(), kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(Origin1(), kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(Origin2(), kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(OriginExt(), kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(OriginDevTools(), kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(Origin1(), kUnprotected | kProtected, mock_policy.get()));
  EXPECT_TRUE(Match(Origin2(), kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(OriginExt(), kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(OriginDevTools(), kUnprotected | kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(Origin1(), kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(Origin2(), kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(OriginExt(), kUnprotected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(OriginDevTools(), kUnprotected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(Origin1(), kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(Origin2(), kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(OriginExt(), kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(OriginDevTools(), kProtected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(Origin1(), kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(Origin2(), kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(OriginExt(), kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_FALSE(Match(OriginDevTools(), kUnprotected | kProtected | kExtension,
                     mock_policy.get()));
}
#endif

// If extensions are disabled, there is no policy.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasksNoPolicy) {
  EXPECT_TRUE(Match(Origin1(), kUnprotected, nullptr));
  EXPECT_FALSE(Match(OriginExt(), kUnprotected, nullptr));
  EXPECT_FALSE(Match(OriginDevTools(), kUnprotected, nullptr));

  EXPECT_FALSE(Match(Origin1(), kProtected, nullptr));
  EXPECT_FALSE(Match(OriginExt(), kProtected, nullptr));
  EXPECT_FALSE(Match(OriginDevTools(), kProtected, nullptr));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(Match(Origin1(), kExtension, nullptr));
  EXPECT_TRUE(Match(OriginExt(), kExtension, nullptr));
  EXPECT_FALSE(Match(OriginDevTools(), kExtension, nullptr));
#endif
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache_NoService) {
  ClearReportingCacheTester tester(network_context(), false);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, true);

  // Nothing to check, since there's no mock service; we're just making sure
  // nothing crashes without a service.
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache) {
  ClearReportingCacheTester tester(network_context(), true);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, true);

  EXPECT_EQ(0, tester.mock().remove_calls());
  EXPECT_EQ(1, tester.mock().remove_all_calls());
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            tester.mock().last_data_type_mask());
  EXPECT_TRUE(ProbablySameFilters(base::RepeatingCallback<bool(const GURL&)>(),
                                  tester.mock().last_origin_filter()));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_ReportingCache_WithFilter) {
  ClearReportingCacheTester tester(network_context(), true);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, builder->Copy());

  EXPECT_EQ(1, tester.mock().remove_calls());
  EXPECT_EQ(0, tester.mock().remove_all_calls());
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            tester.mock().last_data_type_mask());
  EXPECT_TRUE(ProbablySameFilters(builder->BuildUrlFilter(),
                                  tester.mock().last_origin_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, NetworkErrorLogging_NoDelegate) {
  ClearNetworkErrorLoggingTester tester(network_context(), false);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, true);

  // Nothing to check, since there's no mock service; we're just making sure
  // nothing crashes without a service.
}

// This would use an origin filter, but history isn't yet filterable.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, NetworkErrorLogging_History) {
  ClearNetworkErrorLoggingTester tester(network_context(), true);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, true);

  EXPECT_EQ(0, tester.mock().remove_calls());
  EXPECT_EQ(1, tester.mock().remove_all_calls());
  EXPECT_TRUE(ProbablySameFilters(base::RepeatingCallback<bool(const GURL&)>(),
                                  tester.mock().last_origin_filter()));
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

// Test that all WebsiteSettings are getting deleted by creating a
// value for each of them and removing data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AllTypesAreGettingDeleted) {
  TestingProfile* profile = GetProfile();
  ASSERT_TRUE(SubresourceFilterProfileContextFactory::GetForProfile(profile));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  auto* registry = content_settings::WebsiteSettingsRegistry::GetInstance();
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  auto* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  // Create a safe_browsing::VerdictCacheManager that will handle deletion of
  // ContentSettingsType::PASSWORD_PROTECTION entries.
  safe_browsing::VerdictCacheManager sb_cache_manager(history_service, map);

  GURL url("https://example.com");

  // List of types that don't have to be deletable.
  static const ContentSettingsType non_deletable_types[] = {
      // Doesn't allow any values.
      ContentSettingsType::PROTOCOL_HANDLERS,
      // Doesn't allow any values.
      ContentSettingsType::MIXEDSCRIPT,
      // Only policy provider sets exceptions for this type.
      ContentSettingsType::AUTO_SELECT_CERTIFICATE,

      // TODO(710873): Make sure that these get fixed:
      // Not deleted but should be deleted with history?
      ContentSettingsType::IMPORTANT_SITE_INFO,
  };

  // Set a value for every WebsiteSetting.
  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    if (base::Contains(non_deletable_types, info->type()))
      continue;
    base::Value some_value;
    auto* content_setting = content_setting_registry->Get(info->type());
    if (content_setting) {
      // Content Settings only allow integers.
      if (content_setting->IsSettingValid(CONTENT_SETTING_ALLOW)) {
        some_value = base::Value(CONTENT_SETTING_ALLOW);
      } else {
        ASSERT_TRUE(content_setting->IsSettingValid(CONTENT_SETTING_ASK));
        some_value = base::Value(CONTENT_SETTING_ASK);
      }
      ASSERT_TRUE(content_setting->IsDefaultSettingValid(CONTENT_SETTING_BLOCK))
          << info->name();
      // Set default to BLOCK to be able to differentiate an exception from the
      // default.
      map->SetDefaultContentSetting(info->type(), CONTENT_SETTING_BLOCK);
    } else {
      // Other website settings only allow dictionaries.
      base::DictionaryValue dict;
      dict.SetKey("foo", base::Value(42));
      some_value = std::move(dict);
    }
    // Create an exception.
    map->SetWebsiteSettingDefaultScope(
        url, url, info->type(), std::string(),
        std::make_unique<base::Value>(some_value.Clone()));

    // Check that the exception was created.
    std::unique_ptr<base::Value> value =
        map->GetWebsiteSetting(url, url, info->type(), std::string(), nullptr);
    EXPECT_TRUE(value) << "Not created: " << info->name();
    if (value)
      EXPECT_EQ(some_value, *value) << "Not created: " << info->name();
  }

  // Delete all data types that trigger website setting deletions.
  uint64_t mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA |
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(), mask, false);

  // All settings should be deleted now.
  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    if (base::Contains(non_deletable_types, info->type()))
      continue;
    std::unique_ptr<base::Value> value =
        map->GetWebsiteSetting(url, url, info->type(), std::string(), nullptr);

    if (value && value->is_int()) {
      EXPECT_EQ(CONTENT_SETTING_BLOCK, value->GetInt())
          << "Not deleted: " << info->name() << " value: " << *value;
    } else {
      EXPECT_FALSE(value) << "Not deleted: " << info->name()
                          << " value: " << *value;
    }
  }
}

#if defined(OS_ANDROID)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, WipeOriginVerifierData) {
  int before =
      customtabs::OriginVerifier::GetClearBrowsingDataCallCountForTesting();
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(
      before + 1,
      customtabs::OriginVerifier::GetClearBrowsingDataCallCountForTesting());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, WipeCrashData) {
#if !defined(OS_CHROMEOS)
  // This test applies only when using a logfile of Crash uploads. Chrome Linux
  // will use Crashpad's database instead of the logfile. Chrome Chrome OS
  // continues to use the logfile even when Crashpad is enabled.
  if (crash_reporter::IsCrashpadEnabled()) {
    GTEST_SKIP();
  }
#endif
  base::FilePath crash_dir_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);

  constexpr char kCrashEntry1[] = "12345,abc\n";
  constexpr char kCrashEntry2[] = "67890,def\n";
  std::string initial_contents = kCrashEntry1;
  initial_contents.append(kCrashEntry2);
  ASSERT_TRUE(base::WriteFile(upload_log_path, initial_contents));

  BlockUntilBrowsingDataRemoved(
      base::Time::FromTimeT(67890u), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  std::string contents;
  base::ReadFileToString(upload_log_path, &contents);
  EXPECT_EQ(kCrashEntry1, contents);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_FALSE(base::PathExists(upload_log_path));
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       WipeNotificationPermissionPromptOutcomesData) {
  PrefService* prefs = GetProfile()->GetPrefs();
  auto* permission_ui_enabler =
      AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(GetProfile());
  base::SimpleTestClock clock_;
  clock_.SetNow(base::Time::Now());
  base::Time first_recorded_time = clock_.Now();

  permission_ui_enabler->set_clock_for_testing(&clock_);
  permission_ui_enabler->RecordPermissionPromptOutcome(
      permissions::PermissionAction::DENIED);
  clock_.Advance(base::TimeDelta::FromDays(1));
  permission_ui_enabler->set_clock_for_testing(&clock_);
  permission_ui_enabler->RecordPermissionPromptOutcome(
      permissions::PermissionAction::DENIED);
  clock_.Advance(base::TimeDelta::FromDays(1));
  base::Time third_recorded_time = clock_.Now();
  permission_ui_enabler->set_clock_for_testing(&clock_);
  permission_ui_enabler->RecordPermissionPromptOutcome(
      permissions::PermissionAction::DENIED);

  constexpr char kNotificationPermissionActionsPrefPath[] =
      "profile.content_settings.permission_actions.notifications";

  EXPECT_EQ(3u,
            prefs->GetList(kNotificationPermissionActionsPrefPath)->GetSize());
  // Remove the first and the second element.
  BlockUntilBrowsingDataRemoved(
      first_recorded_time, third_recorded_time,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA, false);
  // There is only one element left.
  EXPECT_EQ(1u,
            prefs->GetList(kNotificationPermissionActionsPrefPath)->GetSize());
  EXPECT_EQ(
      (util::ValueToTime(prefs->GetList(kNotificationPermissionActionsPrefPath)
                             ->begin()
                             ->FindKey("time")))
          .value_or(base::Time()),
      third_recorded_time);

  // Test we wiped all the elements left.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA, false);
  EXPECT_TRUE(prefs->GetList(kNotificationPermissionActionsPrefPath)->empty());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       GetDomainsForDeferredCookieDeletion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  auto* delegate = GetProfile()->GetBrowsingDataRemoverDelegate();

  auto domains = delegate->GetDomainsForDeferredCookieDeletion(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_ACCOUNT_PASSWORDS);
  EXPECT_EQ(domains.size(), 1u);
  EXPECT_EQ(domains[0], "google.com");

  domains = delegate->GetDomainsForDeferredCookieDeletion(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS);
  EXPECT_EQ(domains.size(), 0u);

  domains = delegate->GetDomainsForDeferredCookieDeletion(
      ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES);
  EXPECT_EQ(domains.size(), 0u);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, LiteVideoClearHistoryData) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  // Both LiteVideo and Lite mode must be enabled for the
  // LiteVideoKeyedService to be created.
  feature_list.InitAndEnableFeature(features::kLiteVideo);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "enable-spdy-proxy-auth");

  LiteVideoKeyedService* lite_video_keyed_service =
      LiteVideoKeyedServiceFactory::GetForProfile(GetProfile());
  lite_video_keyed_service->Initialize(GetProfile()->GetPath());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  histogram_tester.ExpectUniqueSample("LiteVideo.UserBlocklist.ClearBlocklist",
                                      true, 1);
}

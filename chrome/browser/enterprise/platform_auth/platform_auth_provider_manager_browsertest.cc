// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/page_transition_types.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#endif
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"

#if BUILDFLAG(IS_MAC)
#include <optional>
#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/raw_ref.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "content/public/test/test_utils.h"
#endif  // BUILDFLAG(IS_MAC)

using ::testing::_;

namespace enterprise_auth {

class PlatformAuthManagerBrowserTest : public PlatformBrowserTest {
 public:
  PlatformAuthManagerBrowserTest() = default;
  ~PlatformAuthManagerBrowserTest() override = default;

  PlatformAuthManagerBrowserTest(const PlatformAuthManagerBrowserTest&) =
      delete;
  PlatformAuthManagerBrowserTest& operator=(
      const PlatformAuthManagerBrowserTest&) = delete;

 protected:
  void SetManagerEnabled(bool enabled, base::OnceClosure on_complete) {
    PlatformAuthProviderManager::GetInstance().SetEnabled(
        enabled, std::move(on_complete));
  }
};

IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest,
                       DataWithoutOriginFiltering) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/empty.html");

  // Install a mock provider.
  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillOnce(::testing::Return(false));
  EXPECT_CALL(*mock_provider, FetchOrigins(_)).Times(0);

  // Issue a request to the test server and ensure that auth data is collected.
  EXPECT_CALL(*mock_provider, GetData(url, _))
      .WillOnce([](const GURL& request_url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie,
                               "new-cookie=new-cookie-data");
        std::move(callback).Run(std::move(auth_headers));
      });

  // Without origin filtering, every navigation goes through the provider. The
  // browser may start unrelated navigations in the background (e.g. the
  // default search engine prewarm prerender on Android); ignore those.
  EXPECT_CALL(*mock_provider, GetData(::testing::Ne(url), _))
      .WillRepeatedly(
          [](const GURL& request_url,
             PlatformAuthProviderManager::GetDataCallback callback) {
            std::move(callback).Run(net::HttpRequestHeaders());
          });

  // The provider instance will be destroyed when `set_provider` is destroyed.
  EXPECT_CALL(*mock_provider, Die());
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  SetManagerEnabled(true, base::OnceClosure());

  EXPECT_TRUE(content::NavigateToURL(
      chrome_test_utils::GetActiveWebContents(this), url));
}

IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest,
                       DataWithOriginFiltering) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a mock provider.
  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillOnce(::testing::Return(true));
  MockPlatformAuthProvider* unsafe_mock_provider = mock_provider.get();
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  // Enable the manager with no origins configured.
  {
    EXPECT_CALL(*unsafe_mock_provider, FetchOrigins(_))
        .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
          std::move(callback).Run(std::make_unique<std::vector<url::Origin>>());
        });
    base::RunLoop run_loop;
    SetManagerEnabled(true, run_loop.QuitClosure());
    run_loop.Run();
    ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);
  }

  // A request now should not invoke the provider for auth data.
  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // Configure the manager with the embedded test server as the IdP origin.
  {
    EXPECT_CALL(*unsafe_mock_provider, FetchOrigins(_))
        .WillOnce([origin = embedded_test_server()->GetOrigin()](
                      PlatformAuthProvider::FetchOriginsCallback callback) {
          std::move(callback).Run(std::make_unique<std::vector<url::Origin>>(
              std::vector<url::Origin>{origin}));
        });
    base::RunLoop run_loop;
    SetManagerEnabled(true, run_loop.QuitClosure());
    run_loop.Run();
    ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);
  }

  // Issue a request to that origin and ensure that auth data is collected.
  EXPECT_CALL(*unsafe_mock_provider, GetData(_, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie,
                               "new-cookie=new-cookie-data");
        std::move(callback).Run(std::move(auth_headers));
      });
  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // The provider instance will be destroyed when `set_provider` is destroyed.
  EXPECT_CALL(*unsafe_mock_provider, Die());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest, ConcurrentNavigations) {
  base::Lock lock;
  std::map<GURL, std::string> received_cookies;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Filter out extraneous requests (e.g., /favicon.ico) automatically
        // triggered by the browser after page load. Handling them here would
        // cause lock contention and deadlock (DCHECK failure in base::Lock)
        // against the main thread during the verification phase.
        if (request.GetURL().path() != "/title1.html" &&
            request.GetURL().path() != "/title2.html") {
          return nullptr;
        }
        base::AutoLock auto_lock(lock);
        if (request.headers.find("cookie") != request.headers.end()) {
          received_cookies[request.GetURL()] = request.headers.at("cookie");
        }
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillRepeatedly(::testing::Return(false));

  base::RunLoop get_data_run_loop;
  std::map<GURL, PlatformAuthProviderManager::GetDataCallback>
      pending_callbacks;
  EXPECT_CALL(*mock_provider, GetData(_, _))
      .WillRepeatedly([&](const GURL& url,
                          PlatformAuthProviderManager::GetDataCallback cb) {
        pending_callbacks[url] = std::move(cb);
        if (pending_callbacks.size() == 2) {
          get_data_run_loop.Quit();
        }
      });

  MockPlatformAuthProvider* unsafe_mock_provider = mock_provider.get();
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  SetManagerEnabled(true, base::OnceClosure());

  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Start navigation in tab 1.
  content::WebContents* tab1 = chrome_test_utils::GetActiveWebContents(this);
  tab1->GetController().LoadURL(url1, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());

  // Open a second tab and start navigation.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  content::WebContents* tab2 = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_NE(tab1, tab2);
  tab2->GetController().LoadURL(url2, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());

  // Wait until both navigations have reached the provider's GetData().
  get_data_run_loop.Run();

  ASSERT_EQ(pending_callbacks.size(), 2u);
  ASSERT_TRUE(pending_callbacks.find(url1) != pending_callbacks.end());
  ASSERT_TRUE(pending_callbacks.find(url2) != pending_callbacks.end());

  // Resolve request 1 with token 1.
  net::HttpRequestHeaders headers1;
  headers1.SetHeader(net::HttpRequestHeaders::kCookie, "token=Token1");
  std::move(pending_callbacks[url1]).Run(std::move(headers1));

  // Resolve request 2 with token 2.
  net::HttpRequestHeaders headers2;
  headers2.SetHeader(net::HttpRequestHeaders::kCookie, "token=Token2");
  std::move(pending_callbacks[url2]).Run(std::move(headers2));

  // Wait for both tabs to finish loading.
  content::WaitForLoadStop(tab1);
  content::WaitForLoadStop(tab2);

  // Verify that each navigation only sent its own authentication headers.
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(received_cookies[url1], "token=Token1");
    EXPECT_EQ(received_cookies[url2], "token=Token2");
  }

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  EXPECT_CALL(*unsafe_mock_provider, Die());
}
#endif

#if BUILDFLAG(IS_MAC)
namespace {

constexpr char kSsoUrl[] = "https://idp.example/sso";
constexpr char kUnknownId[] = "UNKNOWN_ID";

using RawConfig = CFPreferencesObserver::RawConfig;
using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

// MDM payload served to the prefs handler. `std::nullopt` `urls` simulates a
// missing `URLs` entry.
struct MdmConfig {
  std::string extension_id;
  std::string team_id;
  std::optional<std::vector<std::string>> urls;
};

ScopedPropList StringsToPropList(const std::vector<std::string>& values) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> array(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  for (const std::string& value : values) {
    CFArrayAppendValue(array.get(), base::SysUTF8ToCFStringRef(value).get());
  }
  return array;
}

// Serves the MDM config held by the test fixture instead of reading it from
// the OS.
class FakeCFPreferencesObserver : public CFPreferencesObserver {
 public:
  FakeCFPreferencesObserver(base::RepeatingClosure& on_update,
                            base::RepeatingCallback<RawConfig()> read_config)
      : on_update_(on_update), read_config_(std::move(read_config)) {}

  void Subscribe(base::RepeatingClosure on_update) override {
    *on_update_ = std::move(on_update);
  }

  void Unsubscribe() override { on_update_->Reset(); }

  base::OnceCallback<RawConfig()> GetReadConfigCallback() override {
    return read_config_;
  }

 private:
  const raw_ref<base::RepeatingClosure> on_update_;
  const base::RepeatingCallback<RawConfig()> read_config_;
};

}  // namespace

// Tests how the MDM config read by `ExtensibleEnterpriseSSOPrefsHandler`
// drives `PlatformAuthProviderManager` while `kPingSso` is enabled. The
// override and features are members so they take effect before the
// browser-owned `PlatformAuthPolicyObserver` creates the prefs handler.
class PlatformAuthManagerMdmConfigBrowserTest : public PlatformBrowserTest {
 protected:
  PlatformAuthManagerMdmConfigBrowserTest()
      : cf_prefs_override_(base::BindRepeating(
            &PlatformAuthManagerMdmConfigBrowserTest::CreateCFPrefsObserver,
            base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    // Waits for the config read triggered on startup to be applied.
    content::RunAllTasksUntilIdle();
  }

  std::unique_ptr<CFPreferencesObserver> CreateCFPrefsObserver() {
    return std::make_unique<FakeCFPreferencesObserver>(
        config_update_callback_,
        base::BindRepeating(&PlatformAuthManagerMdmConfigBrowserTest::GetConfig,
                            base::Unretained(this)));
  }

  // Runs on a background sequence when the prefs handler reads the config.
  RawConfig GetConfig() {
    if (!mdm_config_.has_value()) {
      return RawConfig();
    }
    return RawConfig{
        .extension_id = base::SysUTF8ToCFStringRef(mdm_config_->extension_id),
        .team_id = base::SysUTF8ToCFStringRef(mdm_config_->team_id),
        .hosts = ScopedPropList(),
        .urls = mdm_config_->urls.has_value()
                    ? StringsToPropList(mdm_config_->urls.value())
                    : ScopedPropList(),
    };
  }

  // Notifies the prefs handler about a config change and waits until the new
  // config has been applied.
  void UpdateConfigAndWait() {
    ASSERT_TRUE(config_update_callback_);
    config_update_callback_.Run();
    content::RunAllTasksUntilIdle();
  }

  bool IsManagerEnabled() const {
    return PlatformAuthProviderManager::GetInstance().IsEnabled();
  }

  std::optional<MdmConfig> mdm_config_;
  base::RepeatingClosure config_update_callback_;
  base::test::ScopedFeatureList feature_list_{kPingSso};
  policy::ScopedManagementServiceOverrideForTesting platform_management_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL};
  const ScopedCFPreferenceObserverOverride cf_prefs_override_;
};

// Tests that the manager is disabled when no MDM config is present.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       DisabledWithoutConfig) {
  EXPECT_FALSE(IsManagerEnabled());
}

// Tests that the manager is enabled once a config with URLs is delivered.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       EnabledForConfigWithUrls) {
  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::vector<std::string>{kSsoUrl}};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Tests that the manager is enabled for an Okta config with URLs.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       EnabledForOktaConfigWithUrls) {
  mdm_config_ = MdmConfig{.extension_id = kOktaSsoExtensionId,
                          .team_id = kOktaSsoTeamId,
                          .urls = std::vector<std::string>{kSsoUrl}};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Tests that the manager stays disabled for a config from an unknown IdP.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       DisabledForUnknownIdp) {
  mdm_config_ = MdmConfig{.extension_id = kUnknownId,
                          .team_id = kUnknownId,
                          .urls = std::vector<std::string>{kSsoUrl}};
  UpdateConfigAndWait();

  EXPECT_FALSE(IsManagerEnabled());
}

// Tests that the manager is enabled for a known IdP config without a `URLs`
// entry, since the proxying URL loader factory relies on the manager being
// enabled.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       EnabledForConfigWithoutUrls) {
  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::nullopt};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Tests that the manager is enabled for a known IdP config with an empty
// `URLs` entry.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       EnabledForConfigWithEmptyUrls) {
  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::vector<std::string>()};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Tests that the manager is enabled for an Okta config without a `URLs` entry.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       EnabledForOktaConfigWithoutUrls) {
  mdm_config_ = MdmConfig{.extension_id = kOktaSsoExtensionId,
                          .team_id = kOktaSsoTeamId,
                          .urls = std::nullopt};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Tests that the manager gets disabled when the config is removed.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       DisabledWhenConfigRemoved) {
  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::vector<std::string>{kSsoUrl}};
  UpdateConfigAndWait();
  ASSERT_TRUE(IsManagerEnabled());

  mdm_config_.reset();
  UpdateConfigAndWait();

  EXPECT_FALSE(IsManagerEnabled());
}

// Tests that the manager stays enabled when the URLs are removed from the
// config.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigBrowserTest,
                       StaysEnabledWhenUrlsRemoved) {
  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::vector<std::string>{kSsoUrl}};
  UpdateConfigAndWait();
  ASSERT_TRUE(IsManagerEnabled());

  mdm_config_->urls = std::vector<std::string>();
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}

// Serves a valid config from the start, so it is read when the browser-owned
// `PlatformAuthPolicyObserver` creates the prefs handler.
class PlatformAuthManagerMdmConfigAtStartupBrowserTest
    : public PlatformAuthManagerMdmConfigBrowserTest {
 protected:
  PlatformAuthManagerMdmConfigAtStartupBrowserTest() {
    mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                            .team_id = kPingSsoTeamId,
                            .urls = std::vector<std::string>{kSsoUrl}};
  }
};

// Tests that the manager is enabled when a valid config is present on startup.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigAtStartupBrowserTest,
                       EnabledForConfigAtStartup) {
  EXPECT_TRUE(IsManagerEnabled());
}

class PlatformAuthManagerMdmConfigPingSsoDisabledBrowserTest
    : public PlatformAuthManagerMdmConfigBrowserTest {
 protected:
  PlatformAuthManagerMdmConfigPingSsoDisabledBrowserTest() {
    disabled_feature_list_.InitAndDisableFeature(kPingSso);
  }

  base::test::ScopedFeatureList disabled_feature_list_;
};

// Tests that, while `kPingSso` is disabled, the manager doesn't depend on the
// MDM config.
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerMdmConfigPingSsoDisabledBrowserTest,
                       EnabledWithoutConfig) {
  EXPECT_TRUE(IsManagerEnabled());

  mdm_config_ = MdmConfig{.extension_id = kPingSsoExtensionId,
                          .team_id = kPingSsoTeamId,
                          .urls = std::vector<std::string>()};
  UpdateConfigAndWait();

  EXPECT_TRUE(IsManagerEnabled());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace enterprise_auth

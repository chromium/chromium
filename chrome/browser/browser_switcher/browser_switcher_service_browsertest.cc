// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_features.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

namespace browser_switcher {

namespace {

const char kAValidUrl[] = "http://example.com/";
const char kAnInvalidUrl[] = "the quick brown fox jumps over the lazy dog";

const char kSitelistXml[] = R"(
  <rules version="1">
    <docMode>
      <domain docMode="9">docs.google.com</domain>
    </docMode>
  </rules>
)";

const char kOtherSitelistXml[] = R"(
  <rules version="1">
    <docMode>
      <domain docMode="9">yahoo.com</domain>
    </docMode>
  </rules>
)";

// This XML parses differently, depending on the value of the
// BrowserSwitcherParsingMode policy.
const char kParsingModeSensitiveSitelistXml[] = R"(
  <site-list version="1">
    <site url="example.com/grey">
      <open-in>None</open-in>
    </site>
    <site url="example.com/chrome">
      <open-in>MSEdge</open-in>
    </site>
    <site url="example.com/ie">
      <open-in>IE11</open-in>
    </site>
  </site-list>
)";

#if BUILDFLAG(IS_WIN)
const char kYetAnotherSitelistXml[] = R"(
  <rules version="1">
    <docMode>
      <domain docMode="9">greylist.invalid.com</domain>
    </docMode>
  </rules>
)";
#endif

bool ReturnValidXml(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  content::URLLoaderInterceptor::WriteResponse(
      headers, std::string(kSitelistXml), params->client.get());
  return true;
}

bool FailToDownload(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 500 Internal Server Error\n\n";
  content::URLLoaderInterceptor::WriteResponse(headers, "",
                                               params->client.get());
  return true;
}

bool ShouldSwitch(BrowserSwitcherService* service, const GURL& url) {
  return service->sitelist()->ShouldSwitch(url);
}

void SetPolicy(policy::PolicyMap* policies,
               const char* key,
               base::Value value) {
  policies->Set(key, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
}

void EnableBrowserSwitcher(policy::PolicyMap* policies) {
  SetPolicy(policies, policy::key::kBrowserSwitcherEnabled, base::Value(true));
}

}  // namespace

class BrowserSwitcherServiceTest : public InProcessBrowserTest {
 public:
  BrowserSwitcherServiceTest() {
    feature_list_.InitAndEnableFeature(kBrowserSwitcherNoneIsGreylist);
  }
  ~BrowserSwitcherServiceTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    BrowserSwitcherService::SetRefreshDelayForTesting(base::TimeDelta());
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_WIN)
    fake_appdata_dir_ =
        browser()->profile()->GetPath().AppendASCII("FakeAppData");
    ASSERT_TRUE(DirectoryExists(fake_appdata_dir_) ||
                CreateDirectory(fake_appdata_dir_));
    BrowserSwitcherServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            [](base::FilePath cache_dir, content::BrowserContext* context) {
              auto* instance = new BrowserSwitcherServiceWin(
                  Profile::FromBrowserContext(context), cache_dir);
              instance->Init();
              return std::unique_ptr<KeyedService>(instance);
            },
            cache_dir()));
#endif
  }

#if BUILDFLAG(IS_WIN)
  void SetUseIeSitelist(bool use_ie_sitelist) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    SetPolicy(&policies, policy::key::kBrowserSwitcherUseIeSitelist,
              base::Value(use_ie_sitelist));
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }
#endif

  void SetExternalUrl(const std::string& url) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
              base::Value(url));
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void WaitForRefresh() {
    base::RunLoop run_loop;
    GetService()->OnAllRulesetsLoadedForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForActionTimeout() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }

  BrowserSwitcherService* GetService() {
    return BrowserSwitcherServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  policy::MockConfigurationPolicyProvider& policy_provider() {
    return provider_;
  }

#if BUILDFLAG(IS_WIN)
  BrowserSwitcherServiceWin* GetServiceWin() {
    return static_cast<BrowserSwitcherServiceWin*>(GetService());
  }

  void WaitForCacheFile() {
    base::RunLoop run_loop;
    GetServiceWin()->OnCacheFileUpdatedForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForSitelistCacheFile() {
    base::RunLoop run_loop;
    GetServiceWin()->OnSitelistCacheFileUpdatedForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::FilePath& appdata_dir() const { return fake_appdata_dir_; }

  const base::FilePath cache_dir() const {
    return appdata_dir().AppendASCII("Google").AppendASCII("BrowserSwitcher");
  }

  const base::FilePath cache_file_path() const {
    return cache_dir().AppendASCII("cache.dat");
  }

  const base::FilePath sitelist_cache_file_path() const {
    return cache_dir().AppendASCII("sitelistcache.dat");
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

#if BUILDFLAG(IS_WIN)
  base::FilePath fake_appdata_dir_;
#endif
};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalSitelistInvalidUrl) {
  SetExternalUrl(kAnInvalidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (!params->url_request.url.is_valid() ||
            params->url_request.url.spec() == kAnInvalidUrl) {
          *happened = true;
        }
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFetchAndParseAfterStartup) {
  SetExternalUrl(kAValidUrl);

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // Return a different sitelist on refresh.
        const char* sitelist_xml =
            (*counter == 0) ? kSitelistXml : kOtherSitelistXml;
        std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        content::URLLoaderInterceptor::WriteResponse(
            headers, std::string(sitelist_xml), params->client.get());
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFirstFetchFailsButSecondWorks) {
  SetExternalUrl(kAValidUrl);

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // First request fails, but second succeeds.
        if (*counter == 0)
          FailToDownload(params);
        else
          ReturnValidXml(params);
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalListensForPrefChanges) {
  // Start with an invalid URL, so no sitelist.
  SetExternalUrl(kAnInvalidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  auto* service = GetService();

  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetExternalUrl(kAValidUrl);
  WaitForRefresh();
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetExternalUrl(kAnInvalidUrl);
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

// Check that changing the BrowserSwitcherParsingMode policy triggers a
// redownload, and parses the XML with the right ParsingMode.
IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalRedownloadOnParsingModeChange) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
            base::Value(kAValidUrl));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        content::URLLoaderInterceptor::WriteResponse(
            headers, kParsingModeSensitiveSitelistXml, params->client.get());
        return true;
      }));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/grey")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/chrome")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/ie")));
  EXPECT_EQ(3u, service->sitelist()->GetExternalSitelist()->sitelist.size());
  EXPECT_EQ(0u, service->sitelist()->GetExternalSitelist()->greylist.size());
  EXPECT_EQ(
      "!//example.com/grey",
      service->sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
  EXPECT_EQ(
      "//example.com/chrome",
      service->sitelist()->GetExternalSitelist()->sitelist[1]->ToString());
  EXPECT_EQ(
      "//example.com/ie",
      service->sitelist()->GetExternalSitelist()->sitelist[2]->ToString());

  SetPolicy(&policies, policy::key::kBrowserSwitcherParsingMode,
            base::Value(static_cast<int>(ParsingMode::kIESiteListMode)));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/grey")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/chrome")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/ie")));
  EXPECT_EQ(2u, service->sitelist()->GetExternalSitelist()->sitelist.size());
  EXPECT_EQ(1u, service->sitelist()->GetExternalSitelist()->greylist.size());
  EXPECT_EQ(
      "!*://example.com/chrome",
      service->sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
  EXPECT_EQ(
      "*://example.com/ie",
      service->sitelist()->GetExternalSitelist()->sitelist[1]->ToString());
  EXPECT_EQ(
      "*://example.com/grey",
      service->sitelist()->GetExternalSitelist()->greylist[0]->ToString());
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalFileUrl) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath sitelist_path = dir.GetPath().AppendASCII("sitelist.xml");
  base::WriteFile(sitelist_path, kSitelistXml);

  SetExternalUrl(net::FilePathToFileURL(sitelist_path).spec());

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresFailedDownload) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresNonManagedPref) {
  browser()->profile()->GetPrefs()->SetString(prefs::kExternalSitelistUrl,
                                              kAValidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() == kAValidUrl)
          *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalGreylistFetchAndParseAfterStartup) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  auto url_list = base::Value::List().Append("*");
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlList,
            base::Value(std::move(url_list)));
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalGreylistUrl,
            base::Value(kAValidUrl));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  WaitForRefresh();
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       PRE_ExternalCachedForBrowserRestart) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalCachedForBrowserRestart) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  // No timeout here, since we're checking that the rules get applied *before*
  // downloading.
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemSitelistInvalidUrl) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAnInvalidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (!params->url_request.url.is_valid() ||
            params->url_request.url.spec() == kAnInvalidUrl) {
          *happened = true;
        }
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

// TODO(crbug.com/323787135): Times out flakily on CI.
IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       DISABLED_IeemFetchAndParseAfterStartup) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemIgnoresFailedDownload) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemIgnoresNonManagedPref) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() == kAValidUrl)
          *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemListensForPrefChanges) {
  // Start disabled.
  SetUseIeSitelist(false);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  auto* service = GetService();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetUseIeSitelist(true);
  WaitForRefresh();
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetUseIeSitelist(false);
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, WritesPrefsToCacheFile) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kAlternativeBrowserPath,
            base::Value("IExplore.exe"));
  base::Value::List alt_params;
  alt_params.Append("--bogus-flag");
  SetPolicy(&policies, policy::key::kAlternativeBrowserParameters,
            base::Value(std::move(alt_params)));
  SetPolicy(&policies, policy::key::kBrowserSwitcherChromePath,
            base::Value("chrome.exe"));
  base::Value::List chrome_params;
  chrome_params.Append("--force-dark-mode");
  SetPolicy(&policies, policy::key::kBrowserSwitcherChromeParameters,
            base::Value(std::move(chrome_params)));
  base::Value::List url_list;
  url_list.Append("example.com");
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlList,
            base::Value(std::move(url_list)));
  base::Value::List greylist;
  greylist.Append("foo.example.com");
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlGreylist,
            base::Value(std::move(greylist)));
  SetPolicy(&policies, policy::key::kBrowserSwitcherParsingMode,
            base::Value(static_cast<int>(ParsingMode::kIESiteListMode)));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  // Execute everything and check "cache.dat" file contents.
  GetService();
  WaitForCacheFile();

  const char expected_output[] =
      "1\n"
      "IExplore.exe\n"
      "--bogus-flag\n"
      "chrome.exe\n"
      "--force-dark-mode --from-browser-switcher\n"
      "1\n"
      "*://example.com/\n"
      "1\n"
      "*://foo.example.com/\n"
      "ie_sitelist\n";

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string output;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path(), &output));
  EXPECT_EQ(expected_output, output);

  // Check that sitelistcache.dat doesn't exist.
  EXPECT_FALSE(base::PathExists(sitelist_cache_file_path()));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       DISABLED_WritesSitelistsToCacheFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath ieem_sitelist_path =
      dir.GetPath().AppendASCII("ieem_sitelist.xml");
  base::WriteFile(ieem_sitelist_path, kSitelistXml);

  base::FilePath external_sitelist_path =
      dir.GetPath().AppendASCII("external_sitelist.xml");
  base::WriteFile(external_sitelist_path, kOtherSitelistXml);

  base::FilePath external_greylist_path =
      dir.GetPath().AppendASCII("external_greylist.xml");
  base::WriteFile(external_greylist_path, kYetAnotherSitelistXml);

  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
            base::Value(net::FilePathToFileURL(external_sitelist_path).spec()));
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalGreylistUrl,
            base::Value(net::FilePathToFileURL(external_greylist_path).spec()));
  SetPolicy(&policies, policy::key::kBrowserSwitcherUseIeSitelist,
            base::Value(true));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(
      net::FilePathToFileURL(ieem_sitelist_path).spec());

  // Execute everything and check "cache.dat" file contents. It should
  // contain the *union* of both sitelists, not just one of them.
  GetService();
  // LBS will write to cache.dat twice: once before downloading the XML files,
  // and then once after. We're interested in the second state, so wait for 2
  // writes to cache.dat.
  WaitForCacheFile();
  WaitForCacheFile();

  base::FilePath expected_chrome_path;
  base::FilePath::CharType chrome_path[MAX_PATH];
#if BUILDFLAG(IS_WIN)
  ::GetModuleFileName(nullptr, chrome_path, ARRAYSIZE(chrome_path));
  expected_chrome_path = base::FilePath(chrome_path);
#endif
  std::string expected_output = base::StringPrintf(
      "1\n"
      "\n"
      "\n"
      "%s\n"
      "--from-browser-switcher\n"
      "2\n"
      "docs.google.com\n"
      "yahoo.com\n"
      "1\n"
      "greylist.invalid.com\n",
      expected_chrome_path.MaybeAsASCII().c_str());

  std::string output;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path(), &output));
  EXPECT_EQ(expected_output, output);

  // Check that sitelistcache.dat doesn't exist.
  EXPECT_FALSE(base::PathExists(sitelist_cache_file_path()));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       PRE_CacheFileCorrectOnStartup) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and check "cache.dat" file contents.
  GetService();
  // LBS will write to cache.dat twice: once before downloading the XML files,
  // and then once after. We're interested in the second state, so wait for 2
  // writes to cache.dat.
  WaitForCacheFile();
  WaitForCacheFile();

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(cache_file_path()));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, CacheFileCorrectOnStartup) {
  SetUseIeSitelist(true);
  // Never refresh the sitelist. We want to check the state of cache.dat after
  // startup, not after the sitelist is downloaded.
  BrowserSwitcherServiceWin::SetFetchDelayForTesting(base::Hours(24));
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and check "cache.dat" file contents.
  GetService();
  WaitForCacheFile();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(cache_file_path(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(base::PathExists(cache_file_path()));
  ASSERT_TRUE(file.IsValid());
  base::FilePath expected_chrome_path;
  base::FilePath::CharType chrome_path[MAX_PATH];
  ::GetModuleFileName(nullptr, chrome_path, ARRAYSIZE(chrome_path));
  expected_chrome_path = base::FilePath(chrome_path);
  std::string expected_output = base::StringPrintf(
      "1\n"
      "\n"
      "\n"
      "%s\n"
      "--from-browser-switcher\n"
      "1\n"
      "docs.google.com\n"
      "0\n"
      "default\n",
      expected_chrome_path.MaybeAsASCII().c_str());

  std::string output;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path(), &output));
  EXPECT_EQ(expected_output, output);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       DeletesSitelistCacheOnStartup) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(base::CreateDirectory(cache_dir()));
  base::WriteFile(sitelist_cache_file_path(), "");
  ASSERT_TRUE(base::PathExists(sitelist_cache_file_path()));

  // Check that "sitelistcache.dat" got cleaned up on startup.
  GetService();
  WaitForSitelistCacheFile();

  EXPECT_FALSE(base::PathExists(sitelist_cache_file_path()));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, WritesNothingIfDisabled) {
  // No policies configured.

  // Check that "cache.dat" and "sitelistcache.dat" don't exist when LBS is not
  // configured.
  auto* service = GetServiceWin();
  // Need to initialize both RunLoops at the same time to avoid deadlocks
  // depending on which is written first (cache.dat or sitelistcache.dat).
  base::RunLoop cache_run_loop;
  base::RunLoop sitelist_cache_run_loop;
  service->OnCacheFileUpdatedForTesting(cache_run_loop.QuitClosure());
  service->OnSitelistCacheFileUpdatedForTesting(
      sitelist_cache_run_loop.QuitClosure());
  cache_run_loop.Run();
  sitelist_cache_run_loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;

  EXPECT_FALSE(base::PathExists(cache_dir()));
  EXPECT_FALSE(base::PathExists(cache_file_path()));
  EXPECT_FALSE(base::PathExists(sitelist_cache_file_path()));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       DoesNotDeleteIfExtensionIsEnabled) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // No policies configured.

  // LBS extension is installed.
  auto extension =
      extensions::ExtensionBuilder()
          .SetLocation(extensions::mojom::ManifestLocation::kInternal)
          .SetID(kLBSExtensionId)
          .SetManifest(base::Value::Dict()
                           .Set("name", "Legacy Browser Support")
                           .Set("manifest_version", 2)
                           .Set("version", "5.9"))
          .Build();
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  // Cache files already exist.
  ASSERT_TRUE(base::CreateDirectory(cache_dir()));
  base::WriteFile(cache_file_path(), "");
  base::WriteFile(sitelist_cache_file_path(), "");
  ASSERT_TRUE(base::PathExists(cache_file_path()));
  ASSERT_TRUE(base::PathExists(sitelist_cache_file_path()));

  GetService();
  WaitForActionTimeout();
  EXPECT_TRUE(base::PathExists(cache_dir()));
  EXPECT_TRUE(base::PathExists(cache_file_path()));
  EXPECT_TRUE(base::PathExists(sitelist_cache_file_path()));
}
#endif

}  // namespace browser_switcher

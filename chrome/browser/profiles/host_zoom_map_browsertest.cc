// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "url/gurl.h"

using content::HostZoomMap;

namespace {

class ZoomLevelChangeObserver {
 public:
  explicit ZoomLevelChangeObserver(content::BrowserContext* context)
      : message_loop_runner_(new content::MessageLoopRunner) {
    subscription_ = zoom::ZoomEventManager::GetForBrowserContext(context)
                        ->AddZoomLevelChangedCallback(base::Bind(
                            &ZoomLevelChangeObserver::OnZoomLevelChanged,
                            base::Unretained(this)));
  }

  void BlockUntilZoomLevelForHostHasChanged(const std::string& host) {
    while (!std::count(changed_hosts_.begin(), changed_hosts_.end(), host)) {
      message_loop_runner_->Run();
      message_loop_runner_ = new content::MessageLoopRunner;
    }
    changed_hosts_.clear();
  }

 private:
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change) {
    changed_hosts_.push_back(change.host);
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  std::vector<std::string> changed_hosts_;
  std::unique_ptr<content::HostZoomMap::Subscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(ZoomLevelChangeObserver);
};

}  // namespace

class HostZoomMapBrowserTest : public InProcessBrowserTest {
 public:
  HostZoomMapBrowserTest() {}

 protected:
  void SetDefaultZoomLevel(double level) {
    browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);
  }

  double GetZoomLevel(const GURL& url) {
    content::HostZoomMap* host_zoom_map = static_cast<content::HostZoomMap*>(
        content::HostZoomMap::GetDefaultForBrowserContext(
            browser()->profile()));
    return host_zoom_map->GetZoomLevelForHostAndScheme(url.scheme(),
                                                       url.host());
  }

  std::vector<std::string> GetHostsWithZoomLevels() {
    typedef content::HostZoomMap::ZoomLevelVector ZoomLevelVector;
    content::HostZoomMap* host_zoom_map = static_cast<content::HostZoomMap*>(
        content::HostZoomMap::GetDefaultForBrowserContext(
            browser()->profile()));
    content::HostZoomMap::ZoomLevelVector zoom_levels =
        host_zoom_map->GetAllZoomLevels();
    std::vector<std::string> results;
    for (ZoomLevelVector::const_iterator it = zoom_levels.begin();
         it != zoom_levels.end(); ++it)
      results.push_back(it->host);
    return results;
  }

  std::vector<std::string> GetHostsWithZoomLevelsFromPrefs() {
    PrefService* prefs = browser()->profile()->GetPrefs();
    const base::DictionaryValue* dictionaries =
        prefs->GetDictionary(prefs::kPartitionPerHostZoomLevels);
    const base::DictionaryValue* values = NULL;
    std::string partition_key =
        ChromeZoomLevelPrefs::GetPartitionKeyForTesting(base::FilePath());
    dictionaries->GetDictionary(partition_key, &values);
    std::vector<std::string> results;
    if (values) {
      for (base::DictionaryValue::Iterator it(*values);
           !it.IsAtEnd(); it.Advance())
        results.push_back(it.key());
    }
    return results;
  }

  GURL ConstructTestServerURL(const char* url_template) {
    return GURL(base::StringPrintf(
        url_template, embedded_test_server()->port()));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::BasicHttpResponse);
  }

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &HostZoomMapBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  DISALLOW_COPY_AND_ASSIGN(HostZoomMapBrowserTest);
};

#define PARTITION_KEY_PLACEHOLDER "NNN"

class HostZoomMapBrowserTestWithPrefs : public HostZoomMapBrowserTest {
 public:
  explicit HostZoomMapBrowserTestWithPrefs(const std::string& prefs_data)
      : prefs_data_(prefs_data) {}

 private:
  // InProcessBrowserTest:
  bool SetUpUserDataDirectory() override {
    std::replace(prefs_data_.begin(), prefs_data_.end(), '\'', '\"');
    // It seems the hash functions on different platforms can return different
    // values for the same input, so make sure we test with the hash appropriate
    // for the platform.
    std::string partition_key =
        ChromeZoomLevelPrefs::GetPartitionKeyForTesting(base::FilePath());
    std::string partition_key_placeholder(PARTITION_KEY_PLACEHOLDER);
    size_t start_index;
    while ((start_index = prefs_data_.find(partition_key_placeholder)) !=
           std::string::npos) {
      prefs_data_.replace(start_index, partition_key_placeholder.size(),
                          partition_key);
    }

    base::FilePath user_data_directory, path_to_prefs;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    path_to_prefs = user_data_directory
        .AppendASCII(TestingProfile::kTestUserProfileDir)
        .Append(chrome::kPreferencesFilename);
    base::CreateDirectory(path_to_prefs.DirName());
    base::WriteFile(
        path_to_prefs, prefs_data_.c_str(), prefs_data_.size());
    return true;
  }

  std::string prefs_data_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMapBrowserTestWithPrefs);
};

// Zoom-related preferences demonstrating the two problems that
// could be caused by the bug. They incorrectly contain a per-host
// zoom level for the empty host; and a value for 'host1' that only
// differs from the default by epsilon. Neither should have been
// persisted.
const char kSanitizationTestPrefs[] =
    "{'partition': {"
    "   'default_zoom_level': { '" PARTITION_KEY_PLACEHOLDER "': 1.2 },"
    "   'per_host_zoom_levels': {"
    "     '" PARTITION_KEY_PLACEHOLDER "': {"
    "       '': 1.1, 'host1': 1.20001, 'host2': 1.3 }"
    "   }"
    "}}";

#undef PARTITION_KEY_PLACEHOLDER

class HostZoomMapSanitizationBrowserTest
    : public HostZoomMapBrowserTestWithPrefs {
 public:
  HostZoomMapSanitizationBrowserTest()
      : HostZoomMapBrowserTestWithPrefs(kSanitizationTestPrefs) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HostZoomMapSanitizationBrowserTest);
};

// Regression test for crbug.com/437392
IN_PROC_BROWSER_TEST_F(HostZoomMapBrowserTest, ZoomEventsWorkForOffTheRecord) {
  GURL test_url(url::kAboutBlankURL);
  std::string test_host(test_url.host());
  std::string test_scheme(test_url.scheme());
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), test_url);

  content::WebContents* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  content::BrowserContext* context = web_contents->GetBrowserContext();
  EXPECT_TRUE(context->IsOffTheRecord());
  ZoomLevelChangeObserver observer(context);
  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents);

  double new_zoom_level =
      host_zoom_map->GetZoomLevelForHostAndScheme(test_scheme, test_host) + 0.5;
  host_zoom_map->SetZoomLevelForHostAndScheme(test_scheme, test_host,
                                              new_zoom_level);
  observer.BlockUntilZoomLevelForHostHasChanged(test_host);
  EXPECT_EQ(new_zoom_level, host_zoom_map->GetZoomLevelForHostAndScheme(
                                test_scheme, test_host));
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    HostZoomMapBrowserTest,
    WebviewBasedSigninUsesDefaultStoragePartitionForEmbedder) {
  GURL signin_url = signin::GetEmbeddedPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
      signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT, false);
  GURL test_url = ConstructTestServerURL(signin_url.spec().c_str());
  std::string test_host(test_url.host());
  std::string test_scheme(test_url.scheme());
  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents);

  // For the webview based sign-in code, the sign in page uses the default host
  // zoom map.
  HostZoomMap* default_profile_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(browser()->profile());
  EXPECT_EQ(host_zoom_map, default_profile_host_zoom_map);
}
#endif

// Regression test for crbug.com/364399.
IN_PROC_BROWSER_TEST_F(HostZoomMapBrowserTest, ToggleDefaultZoomLevel) {
  const double default_zoom_level = blink::PageZoomFactorToZoomLevel(1.5);

  const char kTestURLTemplate1[] = "http://host1:%u/";
  const char kTestURLTemplate2[] = "http://host2:%u/";

  ZoomLevelChangeObserver observer(browser()->profile());

  GURL test_url1 = ConstructTestServerURL(kTestURLTemplate1);
  ui_test_utils::NavigateToURL(browser(), test_url1);

  SetDefaultZoomLevel(default_zoom_level);
  observer.BlockUntilZoomLevelForHostHasChanged(test_url1.host());
  EXPECT_TRUE(
      blink::PageZoomValuesEqual(default_zoom_level, GetZoomLevel(test_url1)));

  GURL test_url2 = ConstructTestServerURL(kTestURLTemplate2);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(
      blink::PageZoomValuesEqual(default_zoom_level, GetZoomLevel(test_url2)));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::PageZoom::Zoom(web_contents, content::PAGE_ZOOM_OUT);
  observer.BlockUntilZoomLevelForHostHasChanged(test_url2.host());
  EXPECT_FALSE(
      blink::PageZoomValuesEqual(default_zoom_level, GetZoomLevel(test_url2)));

  zoom::PageZoom::Zoom(web_contents, content::PAGE_ZOOM_IN);
  observer.BlockUntilZoomLevelForHostHasChanged(test_url2.host());
  EXPECT_TRUE(
      blink::PageZoomValuesEqual(default_zoom_level, GetZoomLevel(test_url2)));

  // Now both tabs should be at the default zoom level, so there should not be
  // any per-host values saved either to Pref, or internally in HostZoomMap.
  EXPECT_TRUE(GetHostsWithZoomLevels().empty());
  EXPECT_TRUE(GetHostsWithZoomLevelsFromPrefs().empty());
}

// Test that garbage data from crbug.com/364399 is cleared up on startup.
IN_PROC_BROWSER_TEST_F(HostZoomMapSanitizationBrowserTest, ClearOnStartup) {
  EXPECT_THAT(GetHostsWithZoomLevels(), testing::ElementsAre("host2"));
  EXPECT_THAT(GetHostsWithZoomLevelsFromPrefs(), testing::ElementsAre("host2"));
}

// Test four things:
//  1. Host zoom maps of parent profile and child profile are different.
//  2. Child host zoom map inherits zoom level at construction.
//  3. Change of zoom level doesn't propagate from child to parent.
//  4. Change of zoom level propagates from parent to child.
IN_PROC_BROWSER_TEST_F(HostZoomMapBrowserTest,
                       OffTheRecordProfileHostZoomMap) {
  // Constants for test case.
  const std::string host("example.com");
  const double zoom_level_25 = 2.5;
  const double zoom_level_30 = 3.0;
  const double zoom_level_40 = 4.0;

  Profile* parent_profile = browser()->profile();
  Profile* child_profile =
      static_cast<ProfileImpl*>(parent_profile)->GetOffTheRecordProfile();
  HostZoomMap* parent_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(parent_profile);
  ASSERT_TRUE(parent_zoom_map);

  parent_zoom_map->SetZoomLevelForHost(host, zoom_level_25);
  ASSERT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_25);

  // Prepare child host zoom map.
  HostZoomMap* child_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(child_profile);
  ASSERT_TRUE(child_zoom_map);

  // Verify.
  EXPECT_NE(parent_zoom_map, child_zoom_map);

  EXPECT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Child must inherit from parent.";

  child_zoom_map->SetZoomLevelForHost(host, zoom_level_30);
  ASSERT_EQ(
      child_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_30);

  EXPECT_NE(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Child change must not propagate to parent.";

  parent_zoom_map->SetZoomLevelForHost(host, zoom_level_40);
  ASSERT_EQ(
      parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_40);

  EXPECT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Parent change should propagate to child.";
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(HostZoomMapBrowserTest,
                       ParentDefaultZoomPropagatesToIncognitoChild) {
  Profile* parent_profile = browser()->profile();
  Profile* child_profile =
      static_cast<ProfileImpl*>(parent_profile)->GetOffTheRecordProfile();

  double new_default_zoom_level =
      parent_profile->GetZoomLevelPrefs()->GetDefaultZoomLevelPref() + 1.f;
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(parent_profile);
  HostZoomMap* child_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(child_profile);
  ASSERT_TRUE(parent_host_zoom_map);
  ASSERT_TRUE(child_host_zoom_map);
  EXPECT_NE(parent_host_zoom_map, child_host_zoom_map);
  EXPECT_NE(new_default_zoom_level, child_host_zoom_map->GetDefaultZoomLevel());

  parent_profile->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
      new_default_zoom_level);
  EXPECT_EQ(new_default_zoom_level, child_host_zoom_map->GetDefaultZoomLevel());
}

IN_PROC_BROWSER_TEST_F(HostZoomMapBrowserTest, PageScaleIsOneChanged) {
  GURL test_url(url::kAboutBlankURL);
  std::string test_host(test_url.host());

  ui_test_utils::NavigateToURL(browser(), test_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::HostZoomMap::PageScaleFactorIsOne(web_contents));

  ZoomLevelChangeObserver observer(browser()->profile());

  web_contents->SetPageScale(1.5);
  observer.BlockUntilZoomLevelForHostHasChanged(test_host);
  EXPECT_FALSE(content::HostZoomMap::PageScaleFactorIsOne(web_contents));

  web_contents->SetPageScale(1.f);
  observer.BlockUntilZoomLevelForHostHasChanged(test_host);
  EXPECT_TRUE(content::HostZoomMap::PageScaleFactorIsOne(web_contents));
}

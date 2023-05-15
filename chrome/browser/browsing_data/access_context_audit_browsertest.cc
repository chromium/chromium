// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif  // defined (OS_ANDROID)

namespace {

// Use host names that are explicitly included in test certificates.
constexpr char kTopLevelHost[] = "a.test";
constexpr char kEmbeddedHost[] = "b.test";

std::string GetPathWithHostAndPortReplaced(const std::string& original_path,
                                           net::HostPortPair host_port_pair) {
  base::StringPairs replacement_text = {
      {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
  return net::test_server::GetFilePathWithReplacements(original_path,
                                                       replacement_text);
}

// Calls the accessStorage javascript function and awaits its completion for
// each frame in the active web contents for |browser|.
void EnsurePageAccessedStorage(content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(
            content::EvalJs(frame,
                            "(async () => { return await accessStorage();})()")
                .value.GetBool());
      });
}

}  // namespace

class AccessContextAuditBrowserTest : public PlatformBrowserTest {
 public:
  AccessContextAuditBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kClientStorageAccessContextAuditing);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    top_level_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    embedded_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    top_level_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_.Start());
    ASSERT_TRUE(top_level_.Start());
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetAllAccessRecords() {
    base::RunLoop run_loop;
    std::vector<AccessContextAuditDatabase::AccessRecord> records_out;
    AccessContextAuditServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this))
        ->GetAllAccessRecords(base::BindLambdaForTesting(
            [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
              records_out = records;
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return records_out;
  }

  // Navigate to a page that accesses cookies and storage APIs and also embeds
  // a site which also accesses cookies and storage APIs.
  void NavigateToTopLevelPage() {
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), top_level_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
  }

  // Navigate directly to the embedded page.
  void NavigateToEmbeddedPage() {
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), embedded_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
  }

  url::Origin top_level_origin() {
    return url::Origin::Create(top_level_.GetURL(kTopLevelHost, "/"));
  }

  url::Origin embedded_origin() {
    return url::Origin::Create(embedded_.GetURL(kEmbeddedHost, "/"));
  }

  GURL top_level_url() {
    std::string replacement_path = GetPathWithHostAndPortReplaced(
        "/browsing_data/embeds_storage_accessor.html",
        net::HostPortPair::FromURL(embedded_.GetURL(kEmbeddedHost, "/")));
    return top_level_.GetURL(kTopLevelHost, replacement_path);
  }

  GURL embedded_url() {
    return embedded_.GetURL(kEmbeddedHost,
                            "/browsing_data/storage_accessor.html");
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer top_level_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer embedded_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// AccessContextAuditService is not used and being removed (crbug.com/1442450).
// Removing the service requires logic to delete the database. We keep a
// browser test that verifies the creation of the database, so we can use it
// later for testing the decomission logic.
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_PersistRecords) {
  NavigateToTopLevelPage();
  NavigateToEmbeddedPage();
  // Check storage access records have been recorded.
  EXPECT_THAT(GetAllAccessRecords(), ::testing::Not(::testing::IsEmpty()));
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// TODO(crbug.com/1257820): PRE_ tests are not supported on Android.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
#define MAYBE_PersistRecords DISABLED_PersistRecords
#else
#define MAYBE_PersistRecords PersistRecords
#endif
IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, MAYBE_PersistRecords) {
  // Check that records have been persisted across restart.
  EXPECT_THAT(GetAllAccessRecords(), ::testing::Not(::testing::IsEmpty()));
}

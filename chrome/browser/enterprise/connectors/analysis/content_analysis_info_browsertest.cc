// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/enterprise/connectors/test/active_user_test_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/content_area_user_provider.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using ::testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

struct ActiveUserTestCase {
  const char* url;
  std::vector<const char*> emails;
  const char* expected_active_email;
};

std::vector<ActiveUserTestCase> TestCases() {
  return {
      // "/u/<N>/" test cases:
      ActiveUserTestCase{
          .url = "https://mail.google.com/abcd/u/0/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://meet.google.com/abcd/u/1/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://datastudio.google.com/abcd/u/2/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          // The index is out of bounds so we can't tell which of the two
          // accounts is active.
          .expected_active_email = "",
      },
      ActiveUserTestCase{
          .url = "https://sites.google.com/abcd/u/0/efgh/",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://keep.google.com/abcd/u/1/efgh/",
          .emails = {"bar@gmail.com"},
          // Even if the index doesn't match the number of cookies, we select
          // the email when only one is present.
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/u/0/efgh/",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "",
      },

      // "authuser=<N>" test cases:
      ActiveUserTestCase{
          .url = "https://calendar.google.com/?authuser=0",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://drive.google.com/?authuser=1",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://gemini.google.com/?authuser=2",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          // The index is out of bounds so we can't tell which of the two
          // accounts is active.
          .expected_active_email = "",
      },
      ActiveUserTestCase{
          .url = "https://script.google.com/?authuser=0",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://cloudsearch.google.com/?authuser=1",
          .emails = {"bar@gmail.com"},
          // Even if the index doesn't match the number of cookies, we select
          // the email when only one is present.
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/?authuser=0",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "",
      },

      // No index in URL test cases:
      ActiveUserTestCase{
          .url = "https://docs.google.com/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      ActiveUserTestCase{
          .url = "https://console.cloud.google.com/",
          .emails = {"bar@gmail.com"},
          // With only 1 user it has to be the active one.
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
  };
}

class ActiveUserEmailBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ActiveUserTestCase> {
 public:
  ActiveUserEmailBrowserTest() {
    active_user_test_mixin_ = std::make_unique<test::ActiveUserTestMixin>(
        &mixin_host_, this, &embedded_https_test_server(), GetParam().emails);
  }

  GURL url() const { return GURL(GetParam().url); }

  std::string expected_active_email() const {
    return GetParam().expected_active_email;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnterpriseActiveUserDetection};

  std::unique_ptr<test::ActiveUserTestMixin> active_user_test_mixin_;
};

class ActiveUserEmailFeatureDisabledBrowserTest
    : public ActiveUserEmailBrowserTest {
 public:
  ActiveUserEmailFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(kEnterpriseActiveUserDetection);
  }
};

struct ActiveFrameUserTestCase {
  const char* tab_url;
  const char* frame_url;
  std::vector<const char*> emails;
  const char* expected_active_email;
};

std::vector<ActiveFrameUserTestCase> FrameUserTestCases() {
  return {
      // Invalid Workspace tab URL with invalid frame URL test case.
      ActiveFrameUserTestCase{
          .tab_url = "https://bar.baz.com/",
          .frame_url = "https://foo.bar/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      // Valid Workspace tab URL with invalid frame URL test case.
      ActiveFrameUserTestCase{
          .tab_url = "https://mail.google.com/",
          .frame_url = "https://foo.bar/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      // Invalid Workspace tab URL with valid frame URL test case.
      ActiveFrameUserTestCase{
          .tab_url = "https://foo.bar/",
          .frame_url = "https://ogs.google.com/u/0/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      // Valid "/u/<N>/" test cases.
      ActiveFrameUserTestCase{
          .tab_url = "https://gemini.google.com/",
          .frame_url = "https://ogs.google.com/u/0/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveFrameUserTestCase{
          .tab_url = "https://docs.google.com/",
          .frame_url = "https://ogs.google.com/abcd/efgh/u/1/ijkl/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      // Valid "authuser=<N>"/ test cases.
      ActiveFrameUserTestCase{
          .tab_url = "https://gemini.google.com/",
          .frame_url = "https://ogs.google.com/?authuser=0",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveFrameUserTestCase{
          .tab_url = "https://docs.google.com/",
          .frame_url = "https://ogs.google.com/abcd/efgh/ijkl/?authuser=1",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      // Valid URLs with no valid index test cases.
      ActiveFrameUserTestCase{
          .tab_url = "https://docs.google.com/",
          .frame_url = "https://ogs.google.com/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      ActiveFrameUserTestCase{
          .tab_url = "https://gemini.google.com/",
          .frame_url = "https://ogs.google.com/abcd/efgh/ijkl/?authuser=foo",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      ActiveFrameUserTestCase{
          .tab_url = "https://docs.google.com/",
          .frame_url = "https://ogs.google.com/",
          .emails = {"bar@gmail.com"},
          // With only 1 user it has to be the active one.
          .expected_active_email = "bar@gmail.com",
      },
  };
}

class ActiveFrameUserEmailBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ActiveFrameUserTestCase> {
 public:
  ActiveFrameUserEmailBrowserTest() {
    active_user_test_mixin_ = std::make_unique<test::ActiveUserTestMixin>(
        &mixin_host_, this, &embedded_https_test_server(), GetParam().emails);
  }

  GURL tab_url() const { return GURL(GetParam().tab_url); }
  GURL frame_url() const { return GURL(GetParam().frame_url); }

  std::string expected_active_email() const {
    return GetParam().expected_active_email;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnterpriseActiveUserDetection};

  std::unique_ptr<test::ActiveUserTestMixin> active_user_test_mixin_;
};

struct ReferrerChainTestCase {
  std::vector<const char*> referrer_chain;
  std::vector<const char*> emails;
  const char* expected_active_email;
};

std::vector<ReferrerChainTestCase> ReferrerChainTestCases() {
  return {
      ReferrerChainTestCase{
          .referrer_chain = {"https://bar.baz.com/", "https://foo.bar/"},
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
      ReferrerChainTestCase{
          .referrer_chain = {"https://bar.baz.com/",
                             "https://script.google.com/?authuser=0"},
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ReferrerChainTestCase{
          .referrer_chain = {"https://bar.baz.com/",
                             "https://script.google.com/?authuser=1"},
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ReferrerChainTestCase{
          .referrer_chain = {"https://bar.baz.com/",
                             "https://keep.google.com/abcd/u/0/efgh/"},
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ReferrerChainTestCase{
          .referrer_chain = {"https://keep.google.com/abcd/u/1/efgh/",
                             "https://bar.baz.com/"},
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
  };
}

class MockSafeBrowsingNavigationObserverManager
    : public safe_browsing::SafeBrowsingNavigationObserverManager {
 public:
  explicit MockSafeBrowsingNavigationObserverManager(
      PrefService* pref_service,
      content::ServiceWorkerContext* context)
      : safe_browsing::SafeBrowsingNavigationObserverManager(pref_service,
                                                             context),
        notification_context_for_removal_(context) {}

  ~MockSafeBrowsingNavigationObserverManager() override {
    if (notification_context_for_removal_) {
      notification_context_for_removal_->RemoveObserver(this);
      ui::Clipboard::GetForCurrentThread()->RemoveObserver(this);
    }
  }

  MOCK_METHOD4(IdentifyReferrerChainByEventURL,
               safe_browsing::ReferrerChainProvider::AttributionResult(
                   const GURL& event_url,
                   SessionID event_tab_id,
                   int user_gesture_count_limit,
                   safe_browsing::ReferrerChain* out_referrer_chain));

 private:
  raw_ptr<content::ServiceWorkerContext> notification_context_for_removal_;
};

class ReferrerChainActiveUserEmailBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ReferrerChainTestCase> {
 public:
  ReferrerChainActiveUserEmailBrowserTest() {
    active_user_test_mixin_ = std::make_unique<test::ActiveUserTestMixin>(
        &mixin_host_, this, &embedded_https_test_server(), GetParam().emails);
  }

  std::string expected_active_email() const {
    return GetParam().expected_active_email;
  }

  std::unique_ptr<KeyedService> BuildMockNavigationObserverManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    PrefService* pref_service = profile->GetPrefs();
    CHECK(pref_service);
    content::ServiceWorkerContext* service_worker_context =
        profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
    CHECK(service_worker_context);
    auto mock_manager =
        std::make_unique<MockSafeBrowsingNavigationObserverManager>(
            pref_service, service_worker_context);

    safe_browsing::ReferrerChain referrer_chain;
    for (const char* referrer_url : GetParam().referrer_chain) {
      safe_browsing::ReferrerChainEntry referrer_chain_entry;
      referrer_chain_entry.set_url(referrer_url);
      referrer_chain.Add(std::move(referrer_chain_entry));
    }
    EXPECT_CALL(*mock_manager, IdentifyReferrerChainByEventURL(_, _, _, _))
        .WillOnce(DoAll(SetArgPointee<3>(referrer_chain),
                        Return(safe_browsing::ReferrerChainProvider::SUCCESS)));

    return mock_manager;
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    auto* navigation_observer_manager_factory = safe_browsing::
        SafeBrowsingNavigationObserverManagerFactory::GetInstance();
    navigation_observer_manager_factory->SetTestingFactory(
        context, base::BindRepeating(&ReferrerChainActiveUserEmailBrowserTest::
                                         BuildMockNavigationObserverManager,
                                     base::Unretained(this)));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnterpriseActiveUserDetection};

  std::unique_ptr<test::ActiveUserTestMixin> active_user_test_mixin_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ActiveUserEmailBrowserTest, GetActiveUser) {
  active_user_test_mixin_->SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  ASSERT_EQ(expected_active_email(),
            ContentAreaUserProvider::GetUser(browser()->profile(),
                                             /*web_contents=*/nullptr, url()));

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  ASSERT_EQ(expected_active_email(),
            GetActiveContentAreaUser(identity_manager, url()));
  ASSERT_EQ(expected_active_email(),
            GetActiveContentAreaUser(identity_manager, url()));
}

IN_PROC_BROWSER_TEST_P(ActiveUserEmailBrowserTest,
                       GetActiveUser_URLUnsupportedByURLF) {
  // The point of this test is to validate a domain that only the non-URLF
  // getter functions can obtain, so non-google.com test cases are skipped.
  if (url().DomainIs("invalid.case.com")) {
    return;
  }

  // Replace only the host by one not allowlisted by the URLF getter so as to
  // validate that it always return an empty string, while still validating the
  // non-URLF getters can still use the `/u/<id>/` and `authuser=<id>` logic to
  // identify the right user.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("notebooklm.google.com");
  GURL non_urlf_url = url().ReplaceComponents(replace_host);

  active_user_test_mixin_->SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_urlf_url));
  ASSERT_EQ(
      expected_active_email(),
      ContentAreaUserProvider::GetUser(browser()->profile(),
                                       /*web_contents=*/nullptr, non_urlf_url));

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  ASSERT_EQ(expected_active_email(),
            GetActiveContentAreaUser(identity_manager, non_urlf_url));
}

INSTANTIATE_TEST_SUITE_P(,
                         ActiveUserEmailBrowserTest,
                         testing::ValuesIn(TestCases()));

IN_PROC_BROWSER_TEST_P(ActiveUserEmailFeatureDisabledBrowserTest,
                       GetActiveUser) {
  active_user_test_mixin_->SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  ASSERT_TRUE(ContentAreaUserProvider::GetUser(browser()->profile(),
                                               /*web_contents=*/nullptr, url())
                  .empty());
}

INSTANTIATE_TEST_SUITE_P(,
                         ActiveUserEmailFeatureDisabledBrowserTest,
                         testing::ValuesIn(TestCases()));

IN_PROC_BROWSER_TEST_P(ActiveFrameUserEmailBrowserTest, GetActiveUserForFrame) {
  active_user_test_mixin_->SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab_url()));
  ASSERT_EQ(expected_active_email(),
            GetActiveFrameUser(
                IdentityManagerFactory::GetForProfile(browser()->profile()),
                tab_url(), frame_url()));
}

INSTANTIATE_TEST_SUITE_P(,
                         ActiveFrameUserEmailBrowserTest,
                         testing::ValuesIn(FrameUserTestCases()));

IN_PROC_BROWSER_TEST_P(ReferrerChainActiveUserEmailBrowserTest,
                       ContentAreaUserProvider) {
  active_user_test_mixin_->SetFakeCookieValue();

  GURL url("https://docs.google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(ContentAreaUserProvider::GetUser(
                browser()->profile(),
                browser()->tab_strip_model()->GetActiveWebContents(), url),
            expected_active_email());
}

INSTANTIATE_TEST_SUITE_P(,
                         ReferrerChainActiveUserEmailBrowserTest,
                         testing::ValuesIn(ReferrerChainTestCases()));

}  // namespace enterprise_connectors

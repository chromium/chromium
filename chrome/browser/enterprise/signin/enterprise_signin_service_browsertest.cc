// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_signin_service.h"

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

namespace enterprise_signin {

namespace {

using TransportState = syncer::SyncService::TransportState;

const char kAuthUrl[] =
    "https://accounts.google.com/"
    "AddSession?Email=user%40example.com&continue=https%3A%2F%2Fwww.google.com%"
    "2F";
const char kExampleUrl[] = "http://example.com/";

// A boolean with a more explicit meaning.
enum Activation {
  INACTIVE = 0,
  ACTIVE = 1,
};

// Describes a tab's state, for CheckTabs().
struct TabState {
  GURL url;
  Activation active = INACTIVE;

  bool operator==(const TabState& that) const = default;
};

// TabState string representation, for gtest.
void PrintTo(const TabState& tab_state, std::ostream* os) {
  (*os) << "{'" << tab_state.url << "', "
        << (tab_state.active == ACTIVE ? "ACTIVE" : "INACTIVE") << "}";
}

std::unique_ptr<KeyedService> CreateSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class EnterpriseSigninServiceTest : public InteractiveBrowserTest {
 public:
  EnterpriseSigninServiceTest()
      : dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &EnterpriseSigninServiceTest::SetTestingFactories,
                    base::Unretained(this)))) {}
  ~EnterpriseSigninServiceTest() override = default;

  void SetUpOnMainThread() override {
    CHECK(browser());
    Profile* profile = browser()->profile();
    CHECK(profile);

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile));
    EnterpriseSigninServiceFactory::GetInstance()->GetForBrowserContext(
        profile);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                        signin::ConsentLevel::kSync);
    signin::SetRefreshTokenForPrimaryAccount(identity_manager);
    signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

    profile->GetPrefs()->SetInteger(
        prefs::kProfileReauthPrompt,
        static_cast<int>(ProfileReauthPrompt::kPromptInTab));

    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { sync_service_ = nullptr; }

  syncer::TestSyncService& sync_service() { return *sync_service_; }

  auto SetMaxTransportState(TransportState transport_state) {
    return Steps(Do([this, transport_state]() {
      CHECK(sync_service_);
      sync_service_->SetMaxTransportState(transport_state);
      sync_service_->FireStateChanged();
    }));
  }

  auto SetPersistentAuthError() {
    return Steps(Do([this]() {
      CHECK(sync_service_);
      sync_service_->SetPersistentAuthError();
      sync_service_->FireStateChanged();
    }));
  }

  auto NewTab(Browser* browser, GURL url) {
    return Steps(Do([browser, url = std::move(url)]() {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
              ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }));
  }

  auto CheckTabs(Browser* browser, std::vector<TabState> expected_tabs) {
    return Steps(Do([browser, expected_tabs = std::move(expected_tabs)]() {
      TabStripModel* tab_strip = browser->tab_strip_model();
      std::vector<TabState> actual_tabs;
      for (int i = 0; i < tab_strip->count(); i++) {
        actual_tabs.push_back(TabState{
            tab_strip->GetWebContentsAt(i)->GetVisibleURL(),
            tab_strip->active_index() == i ? ACTIVE : INACTIVE,
        });
      }
      EXPECT_THAT(actual_tabs, testing::ElementsAreArray(expected_tabs));
    }));
  }

  auto ActivateTab(Browser* browser, int index) {
    return Steps(Do([browser, index]() {
      browser->tab_strip_model()->ActivateTabAt(index);
    }));
  }

  auto Navigate(Browser* browser, GURL dest) {
    return Steps(Do([browser, dest]() {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, dest));
    }));
  }

 private:
  void SetTestingFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateSyncService));
  }

  raw_ptr<syncer::TestSyncService> sync_service_;
  base::CallbackListSubscription dependency_manager_subscription_;
};

IN_PROC_BROWSER_TEST_F(EnterpriseSigninServiceTest, DoesNothingIfPolicyNotSet) {
  GURL about_blank = GURL(url::kAboutBlankURL);
  browser()->profile()->GetPrefs()->ClearPref(prefs::kProfileReauthPrompt);
  RunTestSequence(
      SetMaxTransportState(TransportState::START_DEFERRED),
      CheckTabs(browser(), {{about_blank, ACTIVE}}),
      // Sync becomes paused. The policy is not set, so this does nothing.
      SetPersistentAuthError(), CheckTabs(browser(), {{about_blank, ACTIVE}}),
      // Sanity check: not observing SyncService.
      Check([this]() {
        EnterpriseSigninService* signin_service =
            EnterpriseSigninServiceFactory::GetInstance()->GetForBrowserContext(
                browser()->profile());
        return !sync_service().HasObserver(signin_service);
      }));
}

IN_PROC_BROWSER_TEST_F(EnterpriseSigninServiceTest, OpensNewTabOnSyncPaused) {
  GURL example_url(kExampleUrl);
  GURL auth_url(kAuthUrl);
  RunTestSequence(SetMaxTransportState(TransportState::START_DEFERRED),
                  Navigate(browser(), example_url),
                  CheckTabs(browser(), {{example_url, ACTIVE}}),
                  // Sync becomes paused. This should open a new tab pointing to
                  // accounts.google.com.
                  SetPersistentAuthError(),
                  CheckTabs(browser(), {{example_url}, {auth_url, ACTIVE}}),
                  // Call OnStateChanged() again, with the same TransportState.
                  // This should do nothing.
                  ActivateTab(browser(), 0),
                  CheckTabs(browser(), {{example_url, ACTIVE}, {auth_url}}),
                  SetPersistentAuthError(),
                  CheckTabs(browser(), {{example_url, ACTIVE}, {auth_url}}));
}

// TODO(nicolaso): Wayland doesn't support programmatically changing window
// activation. This test relies on `browser2` having activation, so it doesn't
// work on Wayland.
#if !BUILDFLAG(IS_OZONE_WAYLAND)
IN_PROC_BROWSER_TEST_F(EnterpriseSigninServiceTest,
                       CurrentlyActiveTabIsAlreadyLoginPage) {
  GURL example_url(kExampleUrl);
  GURL auth_url(kAuthUrl);

  Browser* browser2 = CreateBrowser(browser()->profile());

  RunTestSequence(
      SetMaxTransportState(TransportState::START_DEFERRED),
      Navigate(browser(), example_url), NewTab(browser(), example_url),
      CheckTabs(browser(), {{example_url, ACTIVE}, {example_url}}),
      Navigate(browser2, example_url), NewTab(browser2, auth_url),
      ActivateTab(browser2, 1),
      CheckTabs(browser2, {{example_url}, {auth_url, ACTIVE}}),
      // Sync becomes paused. The currently active tab already points to
      // accounts.google.com, so do nothing.
      SetPersistentAuthError(),
      CheckTabs(browser(), {{example_url, ACTIVE}, {example_url}}),
      CheckTabs(browser2, {{example_url}, {auth_url, ACTIVE}}),
      // Call OnStateChanged() again, with the same TransportState. This is not
      // a TransportState change, so it should do nothing.
      ActivateTab(browser2, 0),
      CheckTabs(browser2, {{example_url, ACTIVE}, {auth_url}}),
      SetPersistentAuthError(),
      CheckTabs(browser(), {{example_url, ACTIVE}, {example_url}}),
      CheckTabs(browser2, {{example_url, ACTIVE}, {auth_url}}));
}
#endif  // !BUILDFLAG(IS_OZONE_WAYLAND)

}  // namespace enterprise_signin

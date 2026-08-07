// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace contextual_tasks {

inline constexpr char kTestUrl[] = "https://example.com";
inline constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";
inline constexpr char kSrpHomepage[] = "https://www.google.com/search";
inline constexpr char kAimHomepage[] = "https://www.google.com/search?udm=50";
inline constexpr char kAimHomepageThinking[] =
    "https://www.google.com/search?nem=143";
inline constexpr char kSrpShopping[] =
    "https://www.google.com/search?udm=28&q=query";
inline constexpr char kSrpUrl[] = "https://google.com/search?q=query";
inline constexpr char kSignOutUrl[] = "https://accounts.google.com/Logout";
inline constexpr char kSrpUrlWithLensQuery[] =
    "https://www.google.com/search?lns_mode=un";

class FakeContextualTasksEligibilityManager
    : public ContextualTasksEligibilityManager {
 public:
  FakeContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service);
  ~FakeContextualTasksEligibilityManager() override;

  void SetIsEligible(bool eligible);

  bool IsEligibleWithoutIdentity() const override;

 protected:
  bool CalculateEligibility() const override;

 private:
  bool is_eligible_ = true;
};

class MockUiServiceForUrlIntercept : public ContextualTasksUiService {
 public:
  explicit MockUiServiceForUrlIntercept(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      AimEligibilityService* aim_eligibility_service,
      signin::IdentityManager* identity_manager);
  ~MockUiServiceForUrlIntercept() override;

  FakeContextualTasksEligibilityManager* GetFakeEligibilityManager();

  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid& task_id,
               omnibox::ChromeAimEntryPoint entry_point),
              (override));
  MOCK_METHOD(void,
              OnNavigationToAiPageIntercepted,
              (const GURL& url,
               base::WeakPtr<tabs::TabInterface> tab,
               bool is_to_new_tab),
              (override));
  MOCK_METHOD(void,
              OnThreadLinkClicked,
              (const GURL& url,
               base::Uuid task_id,
               base::WeakPtr<tabs::TabInterface> tab,
               base::WeakPtr<BrowserWindowInterface> browser,
               const url::Origin& initiator_origin),
              (override));
  MOCK_METHOD(void,
              OnNonThreadNavigationInTab,
              (content::OpenURLParams url_params,
               base::WeakPtr<tabs::TabInterface> tab),
              (override));
  MOCK_METHOD(void,
              OnSearchResultsNavigationInSidePanel,
              (content::OpenURLParams url_params,
               ContextualTasksUIInterface* web_ui_interface),
              (override));
  MOCK_METHOD(bool, IsUrlForPrimaryAccount, (const GURL& url), (override));
  MOCK_METHOD(bool, IsSignedInToBrowserWithValidCredentials, (), (override));
  MOCK_METHOD(void,
              LoadUrlInWebContents,
              (const GURL& url,
               base::WeakPtr<content::WebContents> web_contents),
              (override));
  MOCK_METHOD(void,
              OpenUrl,
              (const content::OpenURLParams& url_params,
               const blink::mojom::WindowFeatures& window_features,
               BrowserWindowInterface* browser),
              (override));

  using ContextualTasksUiService::HandleNavigationImpl;
  bool HandleNavigationImpl(
      content::OpenURLParams url_params,
      content::WebContents* source_contents,
      tabs::TabInterface* tab,
      bool is_from_embedded_page,
      bool from_can_create_window,
      bool is_same_site_or_from_ui,
      bool is_mobile_ua,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<content::GlobalRenderFrameHostToken>&
          initiator_frame_token,
      const blink::mojom::WindowFeatures& window_features) override;
};

inline content::OpenURLParams CreateOpenUrlParams(
    const GURL& url,
    bool is_renderer_initiated,
    ui::PageTransition page_transition =
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL) {
  content::Referrer referrer;
  return content::OpenURLParams(url, referrer,
                                WindowOpenDisposition::CURRENT_TAB,
                                page_transition, is_renderer_initiated);
}

// A matcher that checks that an OpenURLParams object has the specified URL.
MATCHER_P(OpenURLParamsHasUrl, expected_url, "") {
  return arg.url == expected_url;
}

class ContextualTasksUiServiceTestBase
    : public content::RenderViewHostTestHarness {
 public:
  explicit ContextualTasksUiServiceTestBase(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME);
  ~ContextualTasksUiServiceTestBase() override;

  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

 protected:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<MockAimEligibilityService> aim_eligibility_service_;
  // Ownership of `service_for_nav_` is held by ContextualTasksUiServiceFactory,
  // which is registered via SetTestingFactoryAndUse in SetUp(). Therefore, a
  // non-owning raw_ptr is used here to avoid double-free during destruction.
  raw_ptr<MockUiServiceForUrlIntercept> service_for_nav_ = nullptr;
  std::unique_ptr<ContextualTasksUiService> real_service_;
  std::unique_ptr<MockContextualTasksService> contextual_tasks_service_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_TEST_BASE_H_

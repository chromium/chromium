// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"
#include "gmock/gmock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "url/gurl.h"

namespace {

class MockLensSearchController : public lens::TestLensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : lens::TestLensSearchController(tab) {}

  MOCK_METHOD(void,
              OpenLensOverlay,
              (lens::LensOverlayInvocationSource invocation_source,
               bool should_show_csb),
              (override));

  MOCK_METHOD(void,
              StartContextualization,
              (lens::LensOverlayInvocationSource invocation_source),
              (override));
};

}  // namespace

class ChromeAutocompleteProviderClientTest : public InProcessBrowserTest {
 protected:
  ChromeAutocompleteProviderClientTest() {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](tabs::TabInterface& tab) {
              return std::make_unique<MockLensSearchController>(&tab);
            }));
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features*/ {{omnibox::internal::kWebUIOmniboxPopup, {}},
                              {omnibox::internal::kWebUIOmniboxAimPopup, {}},
                              {omnibox::internal::kWebUIOmniboxSimplification,
                               {{omnibox::kShowLensSearchChip.name, "true"}}}},
        // TODO (crbug.com/555239052) - Fix tests when AskG is launched.
        /*disabled_features*/ {omnibox::kWebUIOmniboxAskGAboutThisPage});
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ChromeAutocompleteProviderClientTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto service =
              std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                  *profile->GetPrefs(),
                  TemplateURLServiceFactory::GetForProfile(profile),
                  profile->GetDefaultStoragePartition()
                      ->GetURLLoaderFactoryForBrowserProcess(),
                  IdentityManagerFactory::GetForProfile(profile),
                  AimEligibilityService::Configuration{
                      .is_off_the_record = profile->IsOffTheRecord()});
          ON_CALL(*service, IsAimEligible())
              .WillByDefault(testing::Return(true));
          return service;
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->GetProfile());
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  void TearDownOnMainThread() override {
    client_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  ChromeAutocompleteProviderClient* GetAutocompleteProviderClient() {
    return static_cast<ChromeAutocompleteProviderClient*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->location_bar()
            ->GetOmniboxController()
            ->autocomplete_controller()
            ->autocomplete_provider_client());
  }

  MockLensSearchController* GetLensSearchController() {
    return static_cast<MockLensSearchController*>(
        LensSearchController::From(browser()->GetActiveTabInterface()));
  }

  OmniboxEditModel* GetOmniboxEditModel() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar()
        ->GetOmniboxController()
        ->edit_model();
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->GetProfile()->GetPrimaryOTRProfile(
            /*create_if_needed=*/true));
  }

  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::FakeServiceWorkerContext service_worker_context_;

 private:
  content::TestStoragePartition storage_partition_;
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       OpenLensOverlay_Show) {
  EXPECT_CALL(*GetLensSearchController(),
              OpenLensOverlay(
                  lens::LensOverlayInvocationSource::kOmniboxPageAction, true))
      .Times(1);
  GetAutocompleteProviderClient()->OpenLensOverlay(
      /*show=*/true, lens::LensOverlayInvocationSource::kOmniboxPageAction);
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       OpenLensOverlay_DontShow) {
  EXPECT_CALL(
      *GetLensSearchController(),
      StartContextualization(lens::LensOverlayInvocationSource::kOmnibox))
      .Times(1);
  GetAutocompleteProviderClient()->OpenLensOverlay(
      /*show=*/false, lens::LensOverlayInvocationSource::kOmniboxPageAction);
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  client_->StartServiceWorker(destination_url);
  EXPECT_TRUE(service_worker_context_
                  .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerIfSuggestDisabled) {
  GURL destination_url("https://google.com/search?q=puppies");

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled,
                                                  false);
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

class ChromeAutocompleteProviderClientWithChipTest
    : public ChromeAutocompleteProviderClientTest {
 protected:
  ChromeAutocompleteProviderClientWithChipTest() {
    // Enable the AIM popup (which implies IsAimPopupFeatureEnabled = true) and
    // the Lens Search Chip.
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxAimPopup, {}},
         {omnibox::internal::kWebUIOmniboxSimplification,
          {{omnibox::kShowLensSearchChip.name, "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientWithChipTest,
                       IsOmniboxNextLensSearchChipEnabled) {
  EXPECT_TRUE(
      GetAutocompleteProviderClient()->IsOmniboxNextLensSearchChipEnabled());
  EXPECT_FALSE(
      GetAutocompleteProviderClient()->IsAskGShowChipEnabled());
}

class ChromeAutocompleteProviderClientAskGShowChipTest
    : public ChromeAutocompleteProviderClientTest {
 protected:
  ChromeAutocompleteProviderClientAskGShowChipTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxAimPopup, {}},
         {omnibox::kWebUIOmniboxAskGAboutThisPage,
          {{"Omnibox_AskGShowChip", "true"}}}},
        {omnibox::internal::kWebUIOmniboxSimplification});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientAskGShowChipTest,
                       IsAskGShowChipEnabled) {
  EXPECT_FALSE(
      GetAutocompleteProviderClient()->IsOmniboxNextLensSearchChipEnabled());
  EXPECT_TRUE(
      GetAutocompleteProviderClient()->IsAskGShowChipEnabled());
}

class ChromeAutocompleteProviderClientAskGCoBrowseTest
    : public ChromeAutocompleteProviderClientTest {
 protected:
  ChromeAutocompleteProviderClientAskGCoBrowseTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kWebUIOmniboxAskGAboutThisPage,
          {{"Omnibox_AskGCoBrowse", "true"}}},
         {contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
        {});
  }

  bool IsContextualTasksSidePanelOpen() {
    auto* controller = contextual_tasks::ContextualTasksPanelController::From(
        browser()->GetActiveTabInterface()->GetBrowserWindowInterface());
    return controller && controller->IsPanelOpenForContextualTask();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientAskGCoBrowseTest,
                       OpensSidePanel) {
  // Ensure the active tab is valid.
  ASSERT_TRUE(browser()->GetActiveTabInterface()->GetContents());

  // Lens should NOT be opened.
  EXPECT_CALL(*GetLensSearchController(),
              OpenLensOverlay(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*GetLensSearchController(), StartContextualization(testing::_))
      .Times(0);

  GetAutocompleteProviderClient()->OpenCoBrowsePanel();

  // Verify that the side panel is open.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));

  // Verify that the side panel web contents is focused.
  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser()->GetActiveTabInterface()->GetBrowserWindowInterface());
  ASSERT_TRUE(controller);
  content::WebContents* side_panel_contents =
      controller->GetActiveWebContents();
  ASSERT_TRUE(side_panel_contents);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_contents->ContainsOrIsFocusedWebContents(); }));
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientAskGCoBrowseTest,
                       CreatesContextualSessionHandleWhenNoneExists) {
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents());

  GetAutocompleteProviderClient()->OpenCoBrowsePanel();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));

  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser()->GetActiveTabInterface()->GetBrowserWindowInterface());
  ASSERT_TRUE(controller);
  auto* session_handle = controller->GetContextualSearchSessionHandleForPanel();
  ASSERT_TRUE(session_handle);
  EXPECT_EQ(session_handle->invocation_source(),
            lens::LensOverlayInvocationSource::kOmniboxPageAction);
}

class ChromeAutocompleteProviderClientAskGCoBrowseWithLensOverlayTest
    : public ChromeAutocompleteProviderClientTest {
 protected:
  ChromeAutocompleteProviderClientAskGCoBrowseWithLensOverlayTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kWebUIOmniboxAskGAboutThisPage,
          {{"Omnibox_AskGCoBrowseWithVisualSelection", "true"}}},
         {contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
        {});
  }

  bool IsContextualTasksSidePanelOpen() {
    auto* controller = contextual_tasks::ContextualTasksPanelController::From(
        browser()->GetActiveTabInterface()->GetBrowserWindowInterface());
    return controller && controller->IsPanelOpenForContextualTask();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeAutocompleteProviderClientAskGCoBrowseWithLensOverlayTest,
    OpensSidePanelAndLensOverlaySimultaneously) {
  ASSERT_TRUE(browser()->GetActiveTabInterface()->GetContents());

  // Lens overlay should be opened immediately in parallel with side panel.
  EXPECT_CALL(
      *GetLensSearchController(),
      OpenLensOverlay(lens::LensOverlayInvocationSource::kOmniboxPageAction,
                      testing::_))
      .Times(1);

  // Act: Call the entry point for CoBrowse with visual selection
  GetAutocompleteProviderClient()->OpenCoBrowsePanel();

  // Assert: Verify that the side panel is open.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));
}

class ChromeAutocompleteProviderClientAskGLensChipRouteTest
    : public ChromeAutocompleteProviderClientTest {
 protected:
  ChromeAutocompleteProviderClientAskGLensChipRouteTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kWebUIOmniboxAskGAboutThisPage,
          {{"Omnibox_AskGLensChipRoute", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientAskGLensChipRouteTest,
                       OpenLensOverlay_Show_HideCsb) {
  // When kAskGLensChipRoute is enabled, OpenLensOverlay(true) should pass
  // false for should_show_csb.
  EXPECT_CALL(*GetLensSearchController(),
              OpenLensOverlay(
                  lens::LensOverlayInvocationSource::kOmniboxPageAction, false))
      .Times(1);
  GetAutocompleteProviderClient()->OpenLensOverlay(
      /*show=*/true, lens::LensOverlayInvocationSource::kOmniboxPageAction);
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientAskGLensChipRouteTest,
                       OmniboxEditModelOpenLensSearch_BypassesAction) {
  // When kAskGLensChipRoute is enabled, OpenLensSearch should bypass the action
  // and call OpenLensOverlay directly (which will hide CSB).
  EXPECT_CALL(
      *GetLensSearchController(),
      OpenLensOverlay(lens::LensOverlayInvocationSource::kOmniboxPopupButton,
                      false))
      .Times(1);

  GetOmniboxEditModel()->OpenLensSearch();
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       OmniboxEditModelOpenLensSearch_RoutesToAction) {
  // When kAskGLensChipRoute is disabled (default), OpenLensSearch should route
  // to the action. Since AskG/CoBrowse are disabled by default, it eventually
  // falls back to OpenLensOverlay (showing CSB).
  EXPECT_CALL(*GetLensSearchController(),
              OpenLensOverlay(
                  lens::LensOverlayInvocationSource::kOmniboxPageAction, true))
      .Times(1);

  GetOmniboxEditModel()->OpenLensSearch();
}



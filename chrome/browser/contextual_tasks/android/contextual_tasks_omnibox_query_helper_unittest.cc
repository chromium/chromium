// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_omnibox_query_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/device_info.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
// Completes `ContextualTasksEligibilityManager` for the `std::unique_ptr`
// argument of the `ContextualTasksUiService` constructor.
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/version_info/channel.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace contextual_tasks {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr omnibox::ChromeAimEntryPoint kExpectedEntryPoint =
    omnibox::DESKTOP_CHROME_COBROWSE_OMNIBOX_CONTEXTUAL_SUGGESTION;

class MockTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~MockTabContextualizationController() override = default;

  MOCK_METHOD(void,
              GetPageContext,
              (GetPageContextCallback callback),
              (override));
};

class TestMockContextualTasksUiService : public MockContextualTasksUiService {
 public:
  TestMockContextualTasksUiService(Profile* profile,
                                   ContextualTasksService* service)
      : MockContextualTasksUiService(profile,
                                     service,
                                     /*identity_manager=*/nullptr,
                                     /*aim_eligibility_service=*/nullptr,
                                     /*eligibility_manager=*/nullptr,
                                     /*cookie_synchronizer=*/nullptr) {}
  ~TestMockContextualTasksUiService() override = default;

  MOCK_METHOD(
      void,
      StartTaskUiInSidePanelWithErrorPage,
      (BrowserWindowInterface*,
       tabs::TabInterface*,
       std::unique_ptr<contextual_search::ContextualSearchSessionHandle>,
       omnibox::ChromeAimEntryPoint),
      (override));
};

class DestructionTrackingSessionHandle
    : public NiceMock<contextual_search::MockContextualSearchSessionHandle> {
 public:
  explicit DestructionTrackingSessionHandle(bool* destroyed_flag)
      : destroyed_flag_(destroyed_flag) {}
  ~DestructionTrackingSessionHandle() override {
    if (destroyed_flag_) {
      *destroyed_flag_ = true;
    }
  }

 private:
  raw_ptr<bool> destroyed_flag_;
};

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr,
      /*identity_manager=*/nullptr, AimEligibilityService::Configuration{});
}

std::unique_ptr<KeyedService> BuildMockContextualTasksService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockContextualTasksService>>();
}

std::unique_ptr<KeyedService> BuildMockContextualSearchService(
    content::BrowserContext* context) {
  return std::make_unique<
      NiceMock<contextual_search::MockContextualSearchService>>(
      /*identity_manager=*/nullptr,
      /*url_loader_factory=*/nullptr,
      /*template_url_service=*/nullptr,
      /*variations_client=*/nullptr, version_info::Channel::UNKNOWN,
      /*locale=*/"");
}

std::unique_ptr<lens::ContextualInputData> MakePageContentData(bool eligible) {
  auto data = std::make_unique<lens::ContextualInputData>();
  data->is_page_context_eligible = eligible;
  std::vector<uint8_t> bytes = {'t', 'e', 's', 't'};
  data->context_input.emplace();
  data->context_input->emplace_back(std::move(bytes),
                                    lens::MimeType::kAnnotatedPageContent);
  return data;
}

}  // namespace

class ContextualTasksOmniboxQueryHelperTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualTasksSidePanel, {}},
         {kContextualTasksEntryPoints,
          {{"omnibox_contextual_suggestions", "true"}}},
         {kContextualTasksForceEntryPointEligibility, {}}},
        {});
    base::android::device_info::set_is_desktop_for_testing(true);

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_builder.AddTestingFactory(
        AimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockAimEligibilityService));
    profile_builder.AddTestingFactory(
        ContextualTasksServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockContextualTasksService));
    profile_builder.AddTestingFactory(
        ContextualSearchServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockContextualSearchService));
    profile_ = profile_builder.Build();

    auto* tasks_service =
        ContextualTasksServiceFactory::GetForProfile(profile_.get());
    auto mock_ui_service =
        std::make_unique<NiceMock<TestMockContextualTasksUiService>>(
            profile_.get(), tasks_service);
    mock_ui_service_ = mock_ui_service.get();
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([service = std::move(mock_ui_service)](
                                       content::BrowserContext*) mutable
                                       -> std::unique_ptr<KeyedService> {
          return std::move(service);
        }));

    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    ON_CALL(mock_browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(window_unowned_user_data_host_));
    panel_controller_ =
        std::make_unique<NiceMock<MockContextualTasksPanelController>>(
            &mock_browser_window_);
    entry_point_eligibility_manager_ =
        std::make_unique<EntryPointEligibilityManager>(&mock_browser_window_);

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    tab_weak_factory_ =
        std::make_unique<base::WeakPtrFactory<tabs::TabInterface>>(
            mock_tab_interface_.get());
    ON_CALL(*mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(*mock_tab_interface_, GetWeakPtr())
        .WillByDefault(Return(tab_weak_factory_->GetWeakPtr()));
    ON_CALL(*mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(tab_unowned_user_data_host_));
    ON_CALL(*mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_));
    ON_CALL(*mock_tab_interface_, IsActivated()).WillByDefault(Return(true));

    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents_.get(), mock_tab_interface_.get());
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL("https://example.com/"));
    mock_tab_contextualization_controller_ =
        std::make_unique<NiceMock<MockTabContextualizationController>>(
            mock_tab_interface_.get());
    ON_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
        .WillByDefault([](auto callback) {
          std::move(callback).Run(MakePageContentData(/*eligible=*/true));
        });
  }

  void TearDown() override {
    mock_ui_service_ = nullptr;
    entry_point_eligibility_manager_.reset();
    panel_controller_.reset();
    mock_tab_contextualization_controller_.reset();
    web_contents_.reset();
    tab_weak_factory_.reset();
    mock_tab_interface_.reset();
    profile_.reset();
    base::android::device_info::reset_is_desktop_for_testing();
  }

  contextual_search::MockContextualSearchService* search_service() {
    return static_cast<contextual_search::MockContextualSearchService*>(
        ContextualSearchServiceFactory::GetForProfile(profile_.get()));
  }

  std::unique_ptr<DestructionTrackingSessionHandle> CreateMockSession(
      bool* destroyed_flag = nullptr) {
    auto session =
        std::make_unique<DestructionTrackingSessionHandle>(destroyed_flag);
    ON_CALL(*session, CreateContextToken())
        .WillByDefault(Return(base::UnguessableToken::Create()));
    return session;
  }

  DestructionTrackingSessionHandle* ExpectSessionReturningSearchUrl(
      const GURL& search_url) {
    auto session = CreateMockSession();
    auto* session_ptr = session.get();
    EXPECT_CALL(*session, CreateSearchUrl(_, _))
        .WillOnce([search_url](auto, base::OnceCallback<void(GURL)> callback) {
          std::move(callback).Run(search_url);
        });
    EXPECT_CALL(*search_service(), CreateSession(_, _, _))
        .WillOnce(Return(std::move(session)));
    return session_ptr;
  }

  std::unique_ptr<DestructionTrackingSessionHandle>
  CreateStashedSearchUrlSession(base::OnceCallback<void(GURL)>* callback_out,
                                bool* destroyed_flag = nullptr) {
    auto session = CreateMockSession(destroyed_flag);
    EXPECT_CALL(*session, CreateSearchUrl(_, _))
        .WillOnce([callback_out](auto, base::OnceCallback<void(GURL)> cb) {
          *callback_out = std::move(cb);
        });
    return session;
  }

  void SetPageContextEligible(bool eligible) {
    EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
        .WillOnce([eligible](auto callback) {
          std::move(callback).Run(MakePageContentData(eligible));
        });
  }

  void ExpectDirectTabNavigationWithoutSession(const GURL& suggestion_url) {
    EXPECT_CALL(*search_service(), CreateSession(_, _, _)).Times(0);
    ContextualTasksOmniboxQueryHelper::Start(web_contents_.get(),
                                             suggestion_url);
    ASSERT_NE(web_contents_->GetController().GetPendingEntry(), nullptr);
    EXPECT_EQ(web_contents_->GetController().GetPendingEntry()->GetURL(),
              suggestion_url);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost window_unowned_user_data_host_;
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<MockContextualTasksPanelController> panel_controller_;
  std::unique_ptr<EntryPointEligibilityManager>
      entry_point_eligibility_manager_;
  raw_ptr<TestMockContextualTasksUiService> mock_ui_service_ = nullptr;

  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  std::unique_ptr<base::WeakPtrFactory<tabs::TabInterface>> tab_weak_factory_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MockTabContextualizationController>
      mock_tab_contextualization_controller_;
};

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       SidePanelHappyPath_UploadsContextAndStartsTaskInSidePanel) {
  const GURL suggestion_url(
      "https://www.google.com/search?q=bridge+history&ctxsl_trans=1&aep=1");
  const GURL aim_url("https://www.google.com/search?udm=50&q=bridge+history");
  const base::UnguessableToken expected_token =
      base::UnguessableToken::Create();

  auto session = CreateMockSession();
  auto* session_ptr = session.get();

  bool context_captured = false;
  EXPECT_CALL(*session, NotifySessionStarted()).Times(1);
  EXPECT_CALL(*session, CreateContextToken()).WillOnce([&]() {
    EXPECT_TRUE(context_captured);
    return expected_token;
  });
  EXPECT_CALL(*session, StartTabContextUploadFlow(expected_token, _, _))
      .WillOnce([](const base::UnguessableToken&,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions>) {
        ASSERT_NE(data, nullptr);
        EXPECT_TRUE(data->is_implicit_upload);
        EXPECT_EQ(
            data->upload_type,
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY);
      });
  EXPECT_CALL(*session, CreateSearchUrl(_, _))
      .WillOnce([&](std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateSearchUrlRequestInfo> request_info,
                    base::OnceCallback<void(GURL)> callback) {
        ASSERT_TRUE(request_info);
        EXPECT_EQ(request_info->query_text, "bridge history");
        EXPECT_EQ(request_info->additional_params["ctxsl_trans"], "1");
        EXPECT_FALSE(request_info->additional_params.contains("aep"));
        EXPECT_EQ(request_info->search_url_type,
                  contextual_search::ContextualSearchContextController::
                      SearchUrlType::kAim);
        EXPECT_EQ(request_info->aim_entry_point, kExpectedEntryPoint);
        EXPECT_EQ(request_info->lens_overlay_selection_type,
                  lens::MULTIMODAL_SUGGEST_ZERO_PREFIX);
        EXPECT_EQ(
            request_info->invocation_source,
            lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion);
        std::move(callback).Run(aim_url);
      });

  EXPECT_CALL(
      *search_service(),
      CreateSession(
          _, contextual_search::ContextualSearchSource::kOmnibox,
          std::optional<lens::LensOverlayInvocationSource>(
              lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion)))
      .WillOnce([session = std::move(session)](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            ConfigParams> config_params,
                    contextual_search::ContextualSearchSource,
                    std::optional<lens::LensOverlayInvocationSource>) mutable {
        EXPECT_FALSE(config_params->send_lns_surface);
        EXPECT_TRUE(config_params->enable_viewport_images);
        return std::move(session);
      });

  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([&](auto callback) {
        context_captured = true;
        std::move(callback).Run(MakePageContentData(/*eligible=*/true));
      });

  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(
                                     &mock_browser_window_,
                                     mock_tab_interface_.get(), aim_url, _, _))
      .WillOnce(
          [&](BrowserWindowInterface*, tabs::TabInterface*, const GURL&,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  passed_session,
              StartTaskUiOptions options) {
            EXPECT_EQ(passed_session.get(), session_ptr);
            EXPECT_EQ(options.entry_point, kExpectedEntryPoint);
          });

  ContextualTasksOmniboxQueryHelper::Start(web_contents_.get(), suggestion_url);
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       IneligiblePageContext_ShowsProtectedErrorPageWithoutUploading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"ContextualTasksEnableProtectedPageError", "true"}});

  auto session = CreateMockSession();
  auto* session_ptr = session.get();

  EXPECT_CALL(*session, CreateContextToken()).Times(0);
  EXPECT_CALL(*session, StartTabContextUploadFlow(_, _, _)).Times(0);
  EXPECT_CALL(*session, CreateSearchUrl(_, _)).Times(0);
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(Return(std::move(session)));
  SetPageContextEligible(false);
  EXPECT_CALL(*mock_ui_service_,
              StartTaskUiInSidePanelWithErrorPage(&mock_browser_window_,
                                                  mock_tab_interface_.get(), _,
                                                  kExpectedEntryPoint))
      .WillOnce(
          [&](BrowserWindowInterface*, tabs::TabInterface*,
              std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                  passed_session,
              omnibox::ChromeAimEntryPoint) {
            EXPECT_EQ(passed_session.get(), session_ptr);
          });

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(), GURL("https://www.google.com/search?q=summary"));
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

TEST_F(
    ContextualTasksOmniboxQueryHelperTest,
    IneligiblePageContext_ProtectedErrorDisabled_SkipsUploadAndOpensSidePanel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"ContextualTasksEnableProtectedPageError", "false"}});

  const GURL suggestion_url("https://www.google.com/search?q=summary");
  const GURL aim_url("https://www.google.com/search?udm=50&q=summary");
  auto* session = ExpectSessionReturningSearchUrl(aim_url);
  EXPECT_CALL(*session, CreateContextToken()).Times(0);
  EXPECT_CALL(*session, StartTabContextUploadFlow(_, _, _)).Times(0);
  SetPageContextEligible(false);
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(
                                     &mock_browser_window_,
                                     mock_tab_interface_.get(), aim_url, _, _))
      .Times(1);

  ContextualTasksOmniboxQueryHelper::Start(web_contents_.get(), suggestion_url);
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       ContextualSuggestionsParamDisabled_LoadsSuggestionUrlInTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasksEntryPoints,
      {{"omnibox_contextual_suggestions", "false"}});
  EXPECT_FALSE(
      ContextualTasksOmniboxQueryHelper::IsEligible(web_contents_.get()));
  ExpectDirectTabNavigationWithoutSession(
      GURL("https://www.google.com/search?q=plain+search"));
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       EntryPointsIneligible_LoadsSuggestionUrlInTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kContextualTasksForceEntryPointEligibility);
  EXPECT_FALSE(
      ContextualTasksOmniboxQueryHelper::IsEligible(web_contents_.get()));
  ExpectDirectTabNavigationWithoutSession(
      GURL("https://www.google.com/search?q=ineligible"));
  EXPECT_EQ(
      ContextualTasksOmniboxQueryHelper::FromWebContents(web_contents_.get()),
      nullptr);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       BecomesIneligibleWithQueryInFlight_CancelsQueryAndLoadsInTab) {
  bool session_destroyed = false;
  base::OnceCallback<void(GURL)> pending_search_url_callback;
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(Return(CreateStashedSearchUrlSession(
          &pending_search_url_callback, &session_destroyed)));
  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(), GURL("https://www.google.com/search?q=first"));
  ASSERT_TRUE(pending_search_url_callback);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kContextualTasksForceEntryPointEligibility);
  ExpectDirectTabNavigationWithoutSession(
      GURL("https://www.google.com/search?q=second"));
  EXPECT_TRUE(session_destroyed);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       ContextSharingPolicyDisabled_LoadsSuggestionUrlInTab) {
  profile_->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));
  ExpectDirectTabNavigationWithoutSession(
      GURL("https://www.google.com/search?q=policy+disabled"));
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       MissingSidePanelController_LoadsSuggestionUrlInTab) {
  panel_controller_.reset();
  EXPECT_TRUE(
      ContextualTasksOmniboxQueryHelper::IsEligible(web_contents_.get()));
  ExpectDirectTabNavigationWithoutSession(
      GURL("https://www.google.com/search?q=fallback+in+tab"));
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       EmptySearchUrlCreated_LoadsSuggestionUrlInTab) {
  const GURL suggestion_url(
      "https://www.google.com/search?q=empty+url+fallback");
  ExpectSessionReturningSearchUrl(GURL());
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(_, _, _, _, _))
      .Times(0);

  ContextualTasksOmniboxQueryHelper::Start(web_contents_.get(), suggestion_url);

  ASSERT_NE(web_contents_->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry()->GetURL(),
            suggestion_url);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       TabDeactivatedInFlight_BailsOutWithoutNavigatingOrOpeningSidePanel) {
  auto session = CreateMockSession();
  EXPECT_CALL(*session, CreateSearchUrl(_, _)).Times(0);
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(Return(std::move(session)));
  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([&](auto callback) {
        ON_CALL(*mock_tab_interface_, IsActivated())
            .WillByDefault(Return(false));
        std::move(callback).Run(MakePageContentData(/*eligible=*/true));
      });
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(_, _, _, _, _))
      .Times(0);

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(),
      GURL("https://www.google.com/search?q=switched+tab"));
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       WebContentsDestroyedWhileSearchUrlPending_CleansUpWithoutLeak) {
  bool session_destroyed = false;
  base::OnceCallback<void(GURL)> pending_search_url_callback;
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(Return(CreateStashedSearchUrlSession(
          &pending_search_url_callback, &session_destroyed)));

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(), GURL("https://www.google.com/search?q=closing+tab"));
  ASSERT_TRUE(pending_search_url_callback);
  EXPECT_FALSE(session_destroyed);

  mock_tab_contextualization_controller_.reset();
  web_contents_.reset();
  EXPECT_TRUE(session_destroyed);

  std::move(pending_search_url_callback)
      .Run(GURL("https://www.google.com/search?udm=50&q=closing+tab"));
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       SecondQueryCancelsEarlierInFlightRequest) {
  const GURL aim_url1("https://www.google.com/search?udm=50&q=first");
  const GURL aim_url2("https://www.google.com/search?udm=50&q=second");

  bool first_session_destroyed = false;
  base::OnceCallback<void(GURL)> stale_callback;
  auto session2 = CreateMockSession();
  EXPECT_CALL(*session2, CreateSearchUrl(_, _))
      .WillOnce([&](auto, base::OnceCallback<void(GURL)> callback) {
        std::move(callback).Run(aim_url2);
      });
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(Return(CreateStashedSearchUrlSession(&stale_callback,
                                                     &first_session_destroyed)))
      .WillOnce(Return(std::move(session2)));
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(
                                     &mock_browser_window_,
                                     mock_tab_interface_.get(), aim_url2, _, _))
      .Times(1);

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(), GURL("https://www.google.com/search?q=first"));
  ASSERT_TRUE(stale_callback);
  EXPECT_FALSE(first_session_destroyed);

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(), GURL("https://www.google.com/search?q=second"));
  EXPECT_TRUE(first_session_destroyed);

  std::move(stale_callback).Run(aim_url1);
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

TEST_F(ContextualTasksOmniboxQueryHelperTest,
       PrimaryPageChangedInFlight_CancelsPendingQuery) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents_.get(), GURL("https://example.com/page1"));

  base::OnceCallback<void(GURL)> pending_search_url_callback;
  EXPECT_CALL(*search_service(), CreateSession(_, _, _))
      .WillOnce(
          Return(CreateStashedSearchUrlSession(&pending_search_url_callback)));
  EXPECT_CALL(*mock_ui_service_, StartTaskUiInSidePanelImpl(_, _, _, _, _))
      .Times(0);

  ContextualTasksOmniboxQueryHelper::Start(
      web_contents_.get(),
      GURL("https://www.google.com/search?q=page1+summary"));
  ASSERT_TRUE(pending_search_url_callback);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents_.get(), GURL("https://example.com/page2"));

  std::move(pending_search_url_callback)
      .Run(GURL("https://www.google.com/search?udm=50&q=page1+summary"));
  EXPECT_EQ(web_contents_->GetLastCommittedURL(),
            GURL("https://example.com/page2"));
  EXPECT_EQ(web_contents_->GetController().GetPendingEntry(), nullptr);
}

}  // namespace contextual_tasks

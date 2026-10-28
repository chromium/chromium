// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_thread_observer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_restore_data.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/host_override.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/tab_helper.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"
#endif
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "ui/actions/actions.h"
#include "ui/native_theme/native_theme.h"
#endif

using testing::_;
using testing::Const;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {

namespace {

class MockContextualTasksUiService : public ContextualTasksUiService {
 public:
  MockContextualTasksUiService(ContextualTasksService* controller,
                               AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(
            nullptr,
            std::make_unique<
                testing::NiceMock<MockContextualTasksUiServiceDelegate>>(),
            controller,
            nullptr,
            aim_eligibility_service,
            /*eligibility_manager=*/nullptr,
            /*cookie_synchronizer=*/nullptr) {
    // Restore only AI pages in the side panel by default. Checking other pages,
    // like search results, needs an `AimEligibilityService`.
    ON_CALL(*this, IsRestorableSidePanelUrl(_))
        .WillByDefault([this](const GURL& url) {
          return ContextualTasksUiService::IsAiUrl(url);
        });
  }
  ~MockContextualTasksUiService() override = default;

  bool IsAiUrl(const GURL& url) override {
    if (ContextualTasksUiService::IsAiUrl(url)) {
      return true;
    }
    std::string udm_value;
    return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
           net::GetValueForKeyInQuery(url, "udm", &udm_value) &&
           udm_value == "50";
  }

  MOCK_METHOD(GURL,
              GetContextualTaskUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid& task_id,
               omnibox::ChromeAimEntryPoint entry_point),
              (override));
  MOCK_METHOD(GURL,
              GetDefaultAiPageUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(std::optional<GURL>,
              GetInitialUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(bool,
              HandleNavigation,
              (content::OpenURLParams url_params,
               content::WebContents* source_contents,
               bool is_from_embedded_page,
               bool from_can_create_window,
               bool is_same_site_or_from_ui,
               bool is_mobile_ua,
               const std::optional<url::Origin>& initiator_origin,
               const std::optional<content::GlobalRenderFrameHostToken>&
                   initiator_frame_token,
               const blink::mojom::WindowFeatures& window_features),
              (override));
  MOCK_METHOD(bool, IsRestorableSidePanelUrl, (const GURL& url), (override));
};

class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RefreshContext, (), (override));
  MOCK_METHOD(void,
              SetContextualTasksPanelController,
              (ContextualTasksPanelController*),
              (override));
  MOCK_METHOD(void, AddLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, RemoveLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, ClearAllLocalTabUnderlines, (), (override));
};

#if !BUILDFLAG(IS_ANDROID)
class CountingTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit CountingTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~CountingTabContextualizationController() override = default;

  // lens::TabContextualizationController:
  void GetPageContext(GetPageContextCallback callback) override {
    ++get_page_context_calls_;
    std::move(callback).Run(nullptr);
  }
  int get_page_context_calls() const { return get_page_context_calls_; }

 private:
  int get_page_context_calls_ = 0;
};
#endif

}  // namespace

class ContextualTasksSidePanelCoordinatorTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    profile_ = builder.Build();
#if !BUILDFLAG(IS_ANDROID)
    InitializeActionIdStringMapping();
#endif

    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&ContextualTasksSidePanelCoordinatorTest::
                                CreateMockContextController,
                            base::Unretained(this)));

    aim_eligibility_response_.add_interception_allowed_hosts("*.google.com");
    aim_eligibility_response_.add_interception_allowed_hosts("*.google.co.uk");
    aim_eligibility_response_.add_interception_allowed_paths("/search");
    auto* aim_rule = aim_eligibility_response_.add_aim_detection_url_rule();
    auto* udm_param = aim_rule->add_required_params();
    udm_param->set_key("udm");
    udm_param->set_value("50");
    aim_eligibility_service_ =
        std::make_unique<NiceMock<MockAimEligibilityService>>(
            *profile_->GetPrefs(), nullptr, nullptr, nullptr);
    ON_CALL(*aim_eligibility_service_, GetMostRecentResponse())
        .WillByDefault(ReturnRef(aim_eligibility_response_));
    ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
        .WillByDefault(
            [this](const GURL& url, std::optional<HostOverride> host_override) {
              return aim_eligibility_service_->AimEligibilityService::IsAimUrl(
                  url, host_override);
            });
    ON_CALL(*aim_eligibility_service_, IsAimHost(_, _))
        .WillByDefault(
            [this](const GURL& url, std::optional<HostOverride> host_override) {
              return aim_eligibility_service_->AimEligibilityService::IsAimHost(
                  url, host_override);
            });
    ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
        .WillByDefault([this](const GURL& url) {
          return aim_eligibility_service_
              ->AimEligibilityService::HasAimUrlParams(url);
        });

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(
            [](AimEligibilityService* aim_eligibility_service,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              auto* controller = ContextualTasksServiceFactory::GetForProfile(
                  Profile::FromBrowserContext(context));
              return std::make_unique<NiceMock<MockContextualTasksUiService>>(
                  controller, aim_eligibility_service);
            },
            aim_eligibility_service_.get()));
    // Eagerly instantiate the MockContextualTasksUiService (as is done in
    // production) so it exists when retrieved by the coordinator via
    // GetForBrowserContextIfExists.
    ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get());

    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));

#if !BUILDFLAG(IS_ANDROID)
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(Const(*browser_window_), GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));

    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_.get());
    ON_CALL(*browser_window_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(Const(*browser_window_), GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
#endif

    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    tab_list_ = std::make_unique<NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);

    auto mock_panel_host =
        std::make_unique<NiceMock<MockContextualTasksPanelHost>>();
    ON_CALL(*mock_panel_host, IsPanelInitialized())
        .WillByDefault(testing::Return(true));
    mock_panel_host_ = mock_panel_host.get();

    coordinator_ = std::make_unique<ContextualTasksSidePanelCoordinator>(
        browser_window_.get(), std::move(mock_panel_host),
        &mock_active_task_context_provider_, nullptr);

    // Create a mock tab.
    tabs::TabInterface* tab_ptr = CreateMockTab();

    ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab_ptr));
    ON_CALL(*tab_list_, GetAllTabs()).WillByDefault([this]() {
      std::vector<tabs::TabInterface*> result;
      for (const auto& tab : tabs_) {
        result.push_back(tab.get());
      }
      return result;
    });

    ContextualTask expected_task(base::Uuid::GenerateRandomV4());
    ON_CALL(*mock_controller_, CreateTask())
        .WillByDefault(Return(expected_task));

#if !BUILDFLAG(IS_ANDROID)
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildMockHatsService));
#endif
  }

  void TearDown() override {
    mock_controller_ = nullptr;
    mock_panel_host_ = nullptr;

    coordinator_.reset();
#if !BUILDFLAG(IS_ANDROID)
    if (tab_strip_model_) {
      tab_strip_model_->CloseAllTabs();
    }
    if (tab_strip_model_delegate_) {
      tab_strip_model_delegate_->SetBrowserWindowInterface(nullptr);
    }
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
#endif
    tab_list_registration_.reset();
    tab_list_.reset();
    browser_window_.reset();

    // WebContents must be destroyed before Profile.
    web_contents_list_.clear();
    tabs_.clear();
    mock_tab_user_data_hosts_.clear();

    profile_.reset();
    aim_eligibility_service_.reset();
#if !BUILDFLAG(IS_ANDROID)
    actions::ActionIdMap::ResetMapsForTesting();
#endif
  }

  tabs::TabInterface* CreateMockTab() {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    content::WebContents* web_contents_ptr =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr)
            .release();
    sessions::SessionTabHelper::CreateForWebContents(web_contents_ptr,
                                                     base::NullCallback());

    ON_CALL(*tab, GetContents()).WillByDefault(Return(web_contents_ptr));

    auto host = std::make_unique<ui::UnownedUserDataHost>();
    ON_CALL(*tab, GetUnownedUserDataHost()).WillByDefault(ReturnRef(*host));
    ON_CALL(Const(*tab), GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(*host));
    mock_tab_user_data_hosts_.push_back(std::move(host));

    tabs::TabInterface* tab_ptr = tab.get();
    tabs_.push_back(std::move(tab));
    web_contents_list_.push_back(base::WrapUnique(web_contents_ptr));
    return tab_ptr;
  }

  void TriggerOnEligibilityChange(bool is_eligible) {
    coordinator_->OnEligibilityChange(is_eligible);
  }

  void CreateWebContentsForTesting() {
    coordinator_->CreateCachedWebContentsForTesting(
        base::Uuid::GenerateRandomV4(), true);
  }

  void CreateCachedWebContentsForTesting(base::Uuid task_id, bool is_open) {
    coordinator_->CreateCachedWebContentsForTesting(task_id, is_open);
  }

  void CreateCachedWebContentsWithUrlForTesting(base::Uuid task_id,
                                                bool is_open,
                                                const GURL& url) {
    std::unique_ptr<content::WebContents> wc =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContents* wc_ptr = wc.get();
    content::WebContentsTester::For(wc_ptr)->NavigateAndCommit(url);
    content::WebContentsTester::For(wc_ptr)->SetTitle(u"");
    coordinator_->task_id_to_web_contents_cache_[task_id] = std::make_unique<
        ContextualTasksSidePanelCoordinator::WebContentsCacheItem>(
        std::move(wc), is_open, base::NullCallback());
  }

  void SetCacheItemLastActiveTimeForTesting(
      base::Uuid task_id,
      base::TimeTicks last_active_time_ticks) {
    auto it = coordinator_->task_id_to_web_contents_cache_.find(task_id);
    if (it != coordinator_->task_id_to_web_contents_cache_.end()) {
      it->second->last_active_time_ticks = last_active_time_ticks;
    }
  }

  void CleanUpUnusedWebContents() { coordinator_->CleanUpUnusedWebContents(); }

  bool UpdateWebContentsForActiveTab() {
    return coordinator_->UpdateWebContentsForActiveTab();
  }

  void ClearCacheForTesting() {
    coordinator_->task_id_to_web_contents_cache_.clear();
  }

  void SetCacheItemEntrySourceForTesting(
      base::Uuid task_id,
      ContextualTasksPanelController::EntrySource entry_source,
      base::TimeTicks open_time_ticks) {
    auto it = coordinator_->task_id_to_web_contents_cache_.find(task_id);
    if (it != coordinator_->task_id_to_web_contents_cache_.end()) {
      it->second->entry_source = entry_source;
      it->second->open_time_ticks = open_time_ticks;
    }
  }

  size_t GetNumberOfActiveTasksForTesting(base::Uuid task_id) {
    return coordinator_->task_id_to_web_contents_cache_.count(task_id);
  }

  content::WebContents* GetWebContentsForTaskForTesting(base::Uuid task_id) {
    auto it = coordinator_->task_id_to_web_contents_cache_.find(task_id);
    return it != coordinator_->task_id_to_web_contents_cache_.end()
               ? it->second->web_contents.get()
               : nullptr;
  }

  bool ShouldBeOpen() { return coordinator_->ShouldBeOpen(); }

  bool IsShowingLensResults() { return coordinator_->IsShowingLensResults(); }

  bool IsShowingAimResults() { return coordinator_->IsShowingAimResults(); }

  void SetActiveTab(tabs::TabInterface* tab) {
    ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab));
  }

  void SetPermissionControllerForTesting(
      std::unique_ptr<ContextualTasksPermissionController> controller) {
    coordinator_->permission_controller_ = std::move(controller);
  }

  std::unique_ptr<KeyedService> CreateMockContextController(
      content::BrowserContext* context) {
    auto mock = std::make_unique<NiceMock<MockContextualTasksService>>();
    mock_controller_ = mock.get();
    return mock;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  void TriggerOnSeeExtensionsClicked() {
    coordinator_->OnSeeExtensionsClicked();
  }

  ContextualTasksExtensionsContainer* GetExtensionsContainer() {
    return coordinator_->extensions_container_.get();
  }
#endif

  // Makes the mock service keep track of tab associations, so that the
  // coordinator sees them change. Only tasks in `existing_tasks_` can be
  // associated with tabs.
  void TrackTabAssociations() {
    ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
        .WillByDefault(
            [this](SessionID tab_id) -> std::optional<ContextualTask> {
              auto it = tab_to_task_.find(tab_id);
              if (it == tab_to_task_.end()) {
                return std::nullopt;
              }
              return ContextualTask(it->second);
            });
    ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
        .WillByDefault([this](const base::Uuid& task_id) {
          std::vector<SessionID> tab_ids;
          for (const auto& [tab_id, associated_task_id] : tab_to_task_) {
            if (associated_task_id == task_id) {
              tab_ids.push_back(tab_id);
            }
          }
          return tab_ids;
        });
    ON_CALL(*mock_controller_, AssociateTabWithTask(_, _))
        .WillByDefault([this](const base::Uuid& task_id, SessionID tab_id) {
          if (existing_tasks_.contains(task_id)) {
            tab_to_task_[tab_id] = task_id;
          }
        });
  }

  // Attaches the side panel state saved when a tab was closed to `tab`, as if
  // it was reopened from recently closed tabs.
  void SetTabRestoreData(tabs::TabInterface* tab,
                         const base::Uuid& task_id,
                         const GURL& thread_url) {
    ContextualTasksTabRestoreData::RestoreFromExtraData(
        tab->GetContents(),
        {{ContextualTasksTabRestoreData::kTaskIdKey,
          task_id.AsLowercaseString()},
         {ContextualTasksTabRestoreData::kThreadUrlKey, thread_url.spec()}});
  }

  void SetPanelClosedOnTab(tabs::TabInterface* tab) {
    coordinator_->tabs_with_closed_panel_.insert(tab->GetHandle());
  }

  MockContextualTasksUiService* GetMockUiService() {
    return static_cast<MockContextualTasksUiService*>(
        ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
#endif
  omnibox::AimEligibilityResponse aim_eligibility_response_;
  std::unique_ptr<NiceMock<MockAimEligibilityService>> aim_eligibility_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<MockTabListInterface> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  NiceMock<MockActiveTaskContextProvider> mock_active_task_context_provider_;
  raw_ptr<MockContextualTasksPanelHost> mock_panel_host_ = nullptr;
  raw_ptr<MockContextualTasksService> mock_controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  std::unique_ptr<ContextualTasksSidePanelCoordinator> coordinator_;

  std::vector<std::unique_ptr<tabs::TabInterface>> tabs_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::vector<std::unique_ptr<ui::UnownedUserDataHost>>
      mock_tab_user_data_hosts_;

  // Used by `TrackTabAssociations()`.
  std::map<SessionID, base::Uuid> tab_to_task_;
  std::set<base::Uuid> existing_tasks_;
};

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OpenSidePanelWillCreateNewTask) {
  // Verify open the side panel with active tab not associated with a task will
  // create a new task.
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(1);
  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, GetActiveEntrySource) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  EXPECT_EQ(ContextualTasksPanelController::EntrySource::kOther,
            coordinator_->GetActiveEntrySource());

  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::
                         DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);

  EXPECT_EQ(ContextualTasksPanelController::EntrySource::kLensOverlay,
            coordinator_->GetActiveEntrySource());

  coordinator_->Show(
      false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);

  EXPECT_EQ(
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton,
      coordinator_->GetActiveEntrySource());

  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  EXPECT_EQ(ContextualTasksPanelController::EntrySource::kOther,
            coordinator_->GetActiveEntrySource());

  coordinator_->Show(
      false, omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_AIO_LINK);

  EXPECT_EQ(ContextualTasksPanelController::EntrySource::kAioToCobr,
            coordinator_->GetActiveEntrySource());

  coordinator_->Show(false, omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE);

  EXPECT_EQ(ContextualTasksPanelController::EntrySource::kAskGoogle,
            coordinator_->GetActiveEntrySource());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, ShowSidePanelSetsEntryPoint) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));
  EXPECT_CALL(*mock_ui_service,
              SetInitialEntryPointForTask(
                  _, omnibox::ChromeAimEntryPoint::
                         DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON))
      .Times(1);
  coordinator_->Show(false, omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShowSidePanelEphemeralToolbarButtonDoesNotSetEntryPoint) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));
  EXPECT_CALL(
      *mock_ui_service,
      SetInitialEntryPointForTask(
          _, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT))
      .Times(1);
  coordinator_->Show(
      false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, CloseClearsLocalTabUnderlines) {
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(1);
  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, CloseSidePanelWhenNotEligible) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  CreateWebContentsForTesting();
  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());

  // Verify that the side panel is closed when not eligible and cache is empty.
  EXPECT_CALL(*mock_panel_host_,
              Close(ContextualTasksPanelHost::AnimationStyle::kStandard));
  TriggerOnEligibilityChange(false);
  EXPECT_EQ(0u, coordinator_->GetNumberOfActiveTasks());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelDiscardsCacheIfNoEntryPoint) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "no-entry-point"}}}},
      {});

  ClearCacheForTesting();

  base::Uuid active_task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(active_task_id, true);

  base::Uuid inactive_task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(inactive_task_id, true);

  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  ContextualTask active_task(active_task_id);
  active_task.AddThread(Thread(ThreadType::kAiMode, "thread_1", "Title", 1000));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(active_task));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(active_task_id))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));
  tabs::TabInterface* inactive_tab = CreateMockTab();
  SessionID inactive_tab_id =
      sessions::SessionTabHelper::IdForTab(inactive_tab->GetContents());
  ContextualTask inactive_task(inactive_task_id);

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(inactive_tab_id))
      .WillRepeatedly(Return(inactive_task));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(inactive_task_id))
      .WillRepeatedly(Return(std::vector<SessionID>{inactive_tab_id}));

  UpdateWebContentsForActiveTab();
  ASSERT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(active_task_id));

  EXPECT_EQ(2u, coordinator_->GetNumberOfActiveTasks());

  EXPECT_CALL(*mock_panel_host_,
              Close(ContextualTasksPanelHost::AnimationStyle::kStandard));
  coordinator_->Close();

  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(active_task_id));
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(inactive_task_id));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelKeepsCacheIfEntryPointSet) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});

  ClearCacheForTesting();

  base::Uuid active_task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(active_task_id, true);

  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(ContextualTask(active_task_id)));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(active_task_id))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  UpdateWebContentsForActiveTab();
  ASSERT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(active_task_id));

  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());

  EXPECT_CALL(*mock_panel_host_,
              Close(ContextualTasksPanelHost::AnimationStyle::kStandard));
  coordinator_->Close();

  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(active_task_id));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShowSidePanelLaunchesSurveyArm1) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"ContextualTasksExpandButtonOptions", "side-panel-expand-button"}});
  base::test::ScopedFeatureList survey_feature_list;
  survey_feature_list.InitAndEnableFeature(
      features::kHappinessTrackingSurveysForDesktopNextPanel);
  profile_->GetPrefs()->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                                   1);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(profile_.get(), true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerNextPanel, 90000, testing::_,
                  testing::Contains(testing::Pair("Experiment ID", "Arm 1"))))
      .Times(1);

  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShowSidePanelLaunchesSurveyArm5) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}});
  base::test::ScopedFeatureList survey_feature_list;
  survey_feature_list.InitAndEnableFeature(
      features::kHappinessTrackingSurveysForDesktopNextPanel);
  profile_->GetPrefs()->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                                   1);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(profile_.get(), true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerNextPanel, 90000, testing::_,
                  testing::Contains(testing::Pair("Experiment ID", "Arm 5"))))
      .Times(1);

  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}
#endif

TEST_F(ContextualTasksSidePanelCoordinatorTest, CleanUpUnusedWebContents) {
  // Clear any tasks from SetUp.
  ClearCacheForTesting();

  // Ensure mock_controller handles any task_id or tab_id by default.
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillRepeatedly(Return(std::vector<SessionID>{}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillRepeatedly(Return(std::nullopt));

  // task_id1: Not associated with any tab. Should be cleaned up immediately.
  base::Uuid task_id1 = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id1, false);

  // task_id2: Associated with the active tab's web contents.
  base::Uuid task_id2 = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id2, true);

  // Ensure the coordinator knows task_id2 is associated with the active tab.
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(ContextualTask(task_id2)));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task_id2))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  // Ensure the coordinator knows task_id2 is the active one.
  UpdateWebContentsForActiveTab();
  ASSERT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(task_id2));

  EXPECT_EQ(2u, coordinator_->GetNumberOfActiveTasks());

  // 1. Clean up task_id1 immediately (no associated tabs).
  CleanUpUnusedWebContents();
  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(task_id1));

  // 2. task_id2 should STILL be there because it's active.
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(task_id2));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, OpenInZeroStateCreatesNewTask) {
  ContextualTask old_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  // Setup: The active tab is currently associated with old_task.
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillOnce(Return(old_task))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(old_task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(old_task.GetTaskId(), active_tab_id))
      .Times(1);
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(1);

  coordinator_->OpenInZeroState();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseZeroStateTaskPreservesTabWhenEphemeralBranded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasksEphemeralBrandedEntryPoint,
      {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}});

  ContextualTask zero_state_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(zero_state_task));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(zero_state_task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(
      *mock_controller_,
      DisassociateTabFromTask(zero_state_task.GetTaskId(), active_tab_id))
      .Times(0);

  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseZeroStateTaskDisassociatesTabInIncognitoWhenEphemeralBranded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasksEphemeralBrandedEntryPoint,
      {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}});

  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ON_CALL(*browser_window_, GetProfile()).WillByDefault(Return(otr_profile));

  ContextualTask zero_state_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(zero_state_task));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(zero_state_task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(
      *mock_controller_,
      DisassociateTabFromTask(zero_state_task.GetTaskId(), active_tab_id))
      .Times(1);

  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseZeroStateTaskDisassociatesTabWhenNotEphemeralBranded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasksEphemeralBrandedEntryPoint,
      {{"ContextualTasksEntryPoint", "no-entry-point"}});

  ContextualTask zero_state_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(zero_state_task));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(zero_state_task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(
      *mock_controller_,
      DisassociateTabFromTask(zero_state_task.GetTaskId(), active_tab_id))
      .Times(1);

  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseTaskWithThreadPreservesTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kContextualTasksEphemeralBrandedEntryPoint,
      {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}});

  ContextualTask thread_task(base::Uuid::GenerateRandomV4());
  thread_task.AddThread(
      Thread(ThreadType::kAiMode, "thread_123", "Title", 1000));
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(thread_task));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(thread_task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(thread_task.GetTaskId(), active_tab_id))
      .Times(0);

  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Close_ScopedToTab_DisassociatesOnlyActiveTab) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id, tab_b_id}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();

  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(task.GetTaskId(), tab_b_id))
      .Times(1);
  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(task.GetTaskId(), tab_a_id))
      .Times(0);

  coordinator_->Close();

  // The panel stays open on the other tab of the task.
  SetActiveTab(tab_a);
  EXPECT_TRUE(ShouldBeOpen());
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(task.GetTaskId()));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Close_ScopedToTab_EphemeralBranded_ClosesPanelOnlyOnActiveTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksSidePanelTabState, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab_a->GetContents()),
          sessions::SessionTabHelper::IdForTab(tab_b->GetContents())}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();

  // Tabs stay associated with the task so that the ephemeral button can
  // resume the thread.
  EXPECT_CALL(*mock_controller_, DisassociateTabFromTask(_, _)).Times(0);

  coordinator_->Close();
  EXPECT_FALSE(ShouldBeOpen());

  // The panel stays open on the other tab of the task.
  SetActiveTab(tab_a);
  EXPECT_TRUE(ShouldBeOpen());

  // Reopening the panel on the tab from the ephemeral toolbar button resumes
  // the same task.
  SetActiveTab(tab_b);
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);
  EXPECT_TRUE(ShouldBeOpen());
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(task.GetTaskId()));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Close_ScopedToTab_EndsSessionWhenClosedOnLastTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksSidePanelTabState, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab_a->GetContents()),
          sessions::SessionTabHelper::IdForTab(tab_b->GetContents())}));

  // Open the panel from the ephemeral toolbar button and interact with it.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);
  coordinator_->OnAiInteraction();

  // Closing the panel on tab A doesn't end the session, since tab B still shows
  // the panel.
  coordinator_->Close();
  histogram_tester.ExpectTotalCount("ContextualTasks.Session.Completed", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 0);

  // Closing the panel on tab B, the last tab that shows it, ends the session.
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();
  coordinator_->Close();
  histogram_tester.ExpectUniqueSample("ContextualTasks.Session.Completed", true,
                                      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 1);
}

TEST_F(
    ContextualTasksSidePanelCoordinatorTest,
    Close_ScopedToTab_ClosedOnAllTabs_KeepsPanelClosedForNewlyAssociatedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksSidePanelTabState, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());

  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetActiveTab(tab_a);
  UpdateWebContentsForActiveTab();

  // Close the panel on the only associated tab of the task.
  coordinator_->Close();
  EXPECT_FALSE(ShouldBeOpen());

  // If a second tab is later associated with the task while no tab has an open
  // panel, switching to that second tab should keep the panel closed.
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id, tab_b_id}));
  SetActiveTab(tab_b);
  EXPECT_FALSE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Close_TabStateDisabled_ClosesPanelOnAllTabsOfTask) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {kContextualTasksSidePanelTabState});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab_a->GetContents()),
          sessions::SessionTabHelper::IdForTab(tab_b->GetContents())}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();

  coordinator_->Close();

  SetActiveTab(tab_a);
  EXPECT_FALSE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTaskChanged_ScopedToTab_RebuildsPanelForOtherTabs) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask old_task(base::Uuid::GenerateRandomV4());
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());

  // The thread was changed on tab B, so only tab B moves to the new task.
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_a_id))
      .WillByDefault(Return(old_task));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_b_id))
      .WillByDefault(Return(new_task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(old_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id}));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(new_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_b_id}));

  CreateCachedWebContentsForTesting(old_task.GetTaskId(), /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  SetActiveTab(tab_b);
  coordinator_->OnTaskChanged(panel_contents, new_task.GetTaskId());

  // The panel WebContents moves to the new task with tab B, and tab A's panel
  // is recreated for its thread right away, so that it's ready when tab A
  // becomes active.
  EXPECT_EQ(panel_contents,
            GetWebContentsForTaskForTesting(new_task.GetTaskId()));
  content::WebContents* rebuilt_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  ASSERT_TRUE(rebuilt_contents);
  EXPECT_NE(panel_contents, rebuilt_contents);

  SetActiveTab(tab_a);
  coordinator_->OnActiveTabChanged(*tab_list_, tab_a);
  EXPECT_EQ(rebuilt_contents, coordinator_->GetActiveWebContents());
  EXPECT_TRUE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTaskChanged_ScopedToTab_PanelClosedOnOtherTabs_WaitsToRebuildPanel) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask old_task(base::Uuid::GenerateRandomV4());
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_a_id))
      .WillByDefault(Return(old_task));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_b_id))
      .WillByDefault(Return(new_task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(old_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id}));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(new_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_b_id}));

  CreateCachedWebContentsForTesting(old_task.GetTaskId(), /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  SetPanelClosedOnTab(tab_a);
  SetActiveTab(tab_b);
  coordinator_->OnTaskChanged(panel_contents, new_task.GetTaskId());

  // Tab A closed the panel, so its panel isn't recreated until it's reopened.
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(old_task.GetTaskId()));
  SetActiveTab(tab_a);
  coordinator_->OnActiveTabChanged(*tab_list_, tab_a);
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(old_task.GetTaskId()));
  EXPECT_FALSE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTaskChanged_TabStateDisabled_DoesNotRebuildPanel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kContextualTasksSidePanelTabState);

  ContextualTask old_task(base::Uuid::GenerateRandomV4());
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_a_id))
      .WillByDefault(Return(old_task));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_b_id))
      .WillByDefault(Return(new_task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(old_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id}));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(new_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_b_id}));

  CreateCachedWebContentsForTesting(old_task.GetTaskId(), /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  SetActiveTab(tab_b);
  coordinator_->OnTaskChanged(panel_contents, new_task.GetTaskId());

  SetActiveTab(tab_a);
  coordinator_->OnActiveTabChanged(*tab_list_, tab_a);
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(old_task.GetTaskId()));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTaskChanged_ScopedToTab_Rearchitecture_RebuildsPanelWithThreadUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kContextualTasksSidePanelTabState,
                                 kContextualTasksSidePanelRearchitecture},
                                {});

  ContextualTask old_task(base::Uuid::GenerateRandomV4());
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_a_id))
      .WillByDefault(Return(old_task));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(tab_b_id))
      .WillByDefault(Return(new_task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(old_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_a_id}));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(new_task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{tab_b_id}));
  const GURL thread_url("https://www.google.com/search?udm=50&mtid=thread1");
  ON_CALL(*mock_controller_,
          GetThreadUrlFromTaskId(old_task.GetTaskId(), _, _, _))
      .WillByDefault([thread_url](const base::Uuid&, const std::string&,
                                  omnibox::ChromeAimEntryPoint,
                                  base::OnceCallback<void(GURL)> callback) {
        std::move(callback).Run(thread_url);
      });

  // The panel shows the old task's WebContents when the thread is changed.
  CreateCachedWebContentsForTesting(old_task.GetTaskId(), /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  UpdateWebContentsForActiveTab();
  ASSERT_EQ(panel_contents, coordinator_->GetActiveWebContents());
  SetActiveTab(tab_b);
  coordinator_->OnTaskChanged(panel_contents, new_task.GetTaskId());

  // The panel WebContents moves to the new task with tab B, and tab A's panel
  // is recreated right away and loads its thread directly.
  EXPECT_EQ(panel_contents,
            GetWebContentsForTaskForTesting(new_task.GetTaskId()));
  content::WebContents* rebuilt_contents =
      GetWebContentsForTaskForTesting(old_task.GetTaskId());
  ASSERT_TRUE(rebuilt_contents);
  EXPECT_NE(panel_contents, rebuilt_contents);
  EXPECT_TRUE(
      ContextualTasksPanelThreadObserver::FromWebContents(rebuilt_contents));
  std::string thread_id;
  EXPECT_TRUE(net::GetValueForKeyInQuery(rebuilt_contents->GetVisibleURL(),
                                         "mtid", &thread_id));
  EXPECT_EQ("thread1", thread_id);

  SetActiveTab(tab_a);
  coordinator_->OnActiveTabChanged(*tab_list_, tab_a);
  EXPECT_EQ(rebuilt_contents, coordinator_->GetActiveWebContents());
  EXPECT_TRUE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       MoveActiveTabToNewThreadIfShared_MovesOnlyActiveTab) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  SessionID tab_a_id =
      sessions::SessionTabHelper::IdForTab(tab_a->GetContents());
  SessionID tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_b->GetContents());

  // Track the tab associations so that the coordinator sees tab B move.
  std::map<SessionID, base::Uuid> tab_to_task = {{tab_a_id, task.GetTaskId()},
                                                 {tab_b_id, task.GetTaskId()}};
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(
          [&tab_to_task](SessionID tab_id) -> std::optional<ContextualTask> {
            auto it = tab_to_task.find(tab_id);
            if (it == tab_to_task.end()) {
              return std::nullopt;
            }
            return ContextualTask(it->second);
          });
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillByDefault([&tab_to_task](const base::Uuid& task_id) {
        std::vector<SessionID> tab_ids;
        for (const auto& [tab_id, associated_task_id] : tab_to_task) {
          if (associated_task_id == task_id) {
            tab_ids.push_back(tab_id);
          }
        }
        return tab_ids;
      });
  ON_CALL(*mock_controller_, DisassociateTabFromTask(_, _))
      .WillByDefault(
          [&tab_to_task](const base::Uuid& task_id, SessionID tab_id) {
            tab_to_task.erase(tab_id);
          });
  ON_CALL(*mock_controller_, AssociateTabWithTask(_, _))
      .WillByDefault(
          [&tab_to_task](const base::Uuid& task_id, SessionID tab_id) {
            tab_to_task[tab_id] = task_id;
          });
  ON_CALL(*mock_controller_, CreateTask()).WillByDefault(Return(new_task));

  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(task.GetTaskId());
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();

  EXPECT_TRUE(coordinator_->MoveActiveTabToNewThreadIfShared());

  // Tab B moves to the new task with its own panel `WebContents`, while tab A
  // keeps the existing panel `WebContents` and its thread.
  EXPECT_EQ(new_task.GetTaskId(), tab_to_task[tab_b_id]);
  EXPECT_EQ(task.GetTaskId(), tab_to_task[tab_a_id]);
  content::WebContents* new_panel_contents =
      GetWebContentsForTaskForTesting(new_task.GetTaskId());
  ASSERT_TRUE(new_panel_contents);
  EXPECT_NE(panel_contents, new_panel_contents);
  EXPECT_EQ(new_panel_contents, coordinator_->GetActiveWebContents());
  EXPECT_EQ(panel_contents, GetWebContentsForTaskForTesting(task.GetTaskId()));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       MoveActiveTabToNewThreadIfShared_PanelClosedOnActiveTab_ReturnsFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksSidePanelTabState, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_b = CreateMockTab();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab_a->GetContents()),
          sessions::SessionTabHelper::IdForTab(tab_b->GetContents())}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetActiveTab(tab_b);
  UpdateWebContentsForActiveTab();
  coordinator_->Close();
  ASSERT_FALSE(ShouldBeOpen());

  EXPECT_CALL(*mock_controller_, DisassociateTabFromTask(_, _)).Times(0);
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(0);

  EXPECT_FALSE(coordinator_->MoveActiveTabToNewThreadIfShared());
  EXPECT_FALSE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       MoveActiveTabToNewThreadIfShared_NotShared_ReturnsFalse) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillByDefault(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab_a->GetContents())}));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);

  EXPECT_CALL(*mock_controller_, DisassociateTabFromTask(_, _)).Times(0);
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(0);

  EXPECT_FALSE(coordinator_->MoveActiveTabToNewThreadIfShared());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_PanelOpen_SavesTask) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);

  std::map<std::string, std::string> data =
      coordinator_->GetTabRestoreData(tab_list_->GetActiveTab());
  EXPECT_EQ(task.GetTaskId().AsLowercaseString(),
            data[ContextualTasksTabRestoreData::kTaskIdKey]);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_Rearchitecture_SavesThreadUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kContextualTasksSidePanelTabState,
                                 kContextualTasksSidePanelRearchitecture},
                                {});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  const GURL thread_url("https://g.ai/search?udm=50&mtid=thread1");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      GetWebContentsForTaskForTesting(task.GetTaskId()), thread_url);

  // The rearchitecture panel shows the thread's AI page directly.
  std::map<std::string, std::string> data =
      coordinator_->GetTabRestoreData(tab_list_->GetActiveTab());
  EXPECT_EQ(task.GetTaskId().AsLowercaseString(),
            data[ContextualTasksTabRestoreData::kTaskIdKey]);
  EXPECT_EQ(thread_url.spec(),
            data[ContextualTasksTabRestoreData::kThreadUrlKey]);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_PanelClosedOnTab_SavesNothing) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);
  SetPanelClosedOnTab(tab_list_->GetActiveTab());

  EXPECT_TRUE(
      coordinator_->GetTabRestoreData(tab_list_->GetActiveTab()).empty());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_TabStateDisabled_SavesNothing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kContextualTasksSidePanelTabState);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  CreateCachedWebContentsForTesting(task.GetTaskId(), /*is_open=*/true);

  EXPECT_TRUE(
      coordinator_->GetTabRestoreData(tab_list_->GetActiveTab()).empty());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_ReopenedTabNotActivated_SavesTaskAndThreadUrl) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  ContextualTask task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(task.GetTaskId());
  const GURL thread_url("https://g.ai/search?udm=50&mtid=thread1");

  // Reopen a tab in the background, so that its panel isn't recreated yet.
  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, task.GetTaskId(), thread_url);
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);
  ASSERT_FALSE(GetWebContentsForTaskForTesting(task.GetTaskId()));

  // Closing the tab again still saves its open panel.
  std::map<std::string, std::string> data =
      coordinator_->GetTabRestoreData(restored_tab);
  EXPECT_EQ(task.GetTaskId().AsLowercaseString(),
            data[ContextualTasksTabRestoreData::kTaskIdKey]);
  EXPECT_EQ(thread_url.spec(),
            data[ContextualTasksTabRestoreData::kThreadUrlKey]);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetTabRestoreData_RecreatedPanelNotLoaded_SavesRestoredUrl) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  ContextualTask task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(task.GetTaskId());
  const GURL thread_url("https://g.ai/search?udm=50&mtid=thread1");

  // Reopen a tab, so that its panel is recreated with the saved page.
  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, task.GetTaskId(), thread_url);
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);
  SetActiveTab(restored_tab);
  coordinator_->OnActiveTabChanged(*tab_list_, restored_tab);
  ASSERT_TRUE(GetWebContentsForTaskForTesting(task.GetTaskId()));

  // Closing the tab again before the panel loads the page still saves it.
  std::map<std::string, std::string> data =
      coordinator_->GetTabRestoreData(restored_tab);
  EXPECT_EQ(task.GetTaskId().AsLowercaseString(),
            data[ContextualTasksTabRestoreData::kTaskIdKey]);
  EXPECT_EQ(thread_url.spec(),
            data[ContextualTasksTabRestoreData::kThreadUrlKey]);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTabAdded_RestoredTab_RejoinsTaskAndRebuildsPanel) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  ContextualTask task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(task.GetTaskId());

  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, task.GetTaskId(), GURL());
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);

  // The reopened tab rejoins its task.
  SessionID restored_tab_id =
      sessions::SessionTabHelper::IdForTab(restored_tab->GetContents());
  EXPECT_EQ(task.GetTaskId(), tab_to_task_[restored_tab_id]);
  EXPECT_FALSE(ContextualTasksTabRestoreData::FromWebContents(
      restored_tab->GetContents()));

  // The panel is recreated when the reopened tab becomes active.
  SetActiveTab(restored_tab);
  coordinator_->OnActiveTabChanged(*tab_list_, restored_tab);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(task.GetTaskId());
  ASSERT_TRUE(panel_contents);
  EXPECT_EQ(panel_contents, coordinator_->GetActiveWebContents());
  EXPECT_TRUE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTabAdded_RestoredTab_TaskDeleted_CreatesTaskForThread) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  const base::Uuid deleted_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(new_task.GetTaskId());
  const GURL thread_url("https://g.ai/search?udm=50&mtid=thread1");
  EXPECT_CALL(*mock_controller_, CreateTaskFromUrl(thread_url))
      .WillOnce(Return(new_task));

  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, deleted_task_id, thread_url);
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);

  // The task was deleted when the tab closed, so the reopened tab gets a new
  // task whose panel loads the saved thread.
  SessionID restored_tab_id =
      sessions::SessionTabHelper::IdForTab(restored_tab->GetContents());
  EXPECT_EQ(new_task.GetTaskId(), tab_to_task_[restored_tab_id]);
  EXPECT_EQ(thread_url, ContextualTasksUiServiceFactory::GetForBrowserContext(
                            profile_.get())
                            ->GetCreationUrlForTask(new_task.GetTaskId()));

  SetActiveTab(restored_tab);
  coordinator_->OnActiveTabChanged(*tab_list_, restored_tab);
  EXPECT_TRUE(GetWebContentsForTaskForTesting(new_task.GetTaskId()));
  EXPECT_TRUE(ShouldBeOpen());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTabAdded_RestoredTab_TaskDeleted_CreatesTaskForLensSearch) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(new_task.GetTaskId());
  const GURL lens_url("https://www.google.com/search?lns_mode=un&q=dog");
  ON_CALL(*GetMockUiService(), IsRestorableSidePanelUrl(lens_url))
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_controller_, CreateTaskFromUrl(lens_url))
      .WillOnce(Return(new_task));

  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, base::Uuid::GenerateRandomV4(), lens_url);
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);

  // The side panel can show search results pages like a Lens search, so the
  // reopened tab gets a new task whose panel loads the saved page.
  SessionID restored_tab_id =
      sessions::SessionTabHelper::IdForTab(restored_tab->GetContents());
  EXPECT_EQ(new_task.GetTaskId(), tab_to_task_[restored_tab_id]);
  EXPECT_EQ(lens_url,
            GetMockUiService()->GetCreationUrlForTask(new_task.GetTaskId()));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTabAdded_RestoredTab_DisallowedUrl_DoesNotCreateTask) {
  base::test::ScopedFeatureList feature_list(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  EXPECT_CALL(*mock_controller_, CreateTaskFromUrl(_)).Times(0);

  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, base::Uuid::GenerateRandomV4(),
                    GURL("https://example.com/"));
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);

  // The side panel can't show the saved page, so it isn't used to recreate the
  // deleted task.
  EXPECT_FALSE(tab_to_task_.contains(
      sessions::SessionTabHelper::IdForTab(restored_tab->GetContents())));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTabAdded_RestoredTab_TabStateDisabled_DoesNotRestorePanel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kContextualTasksSidePanelTabState);
  TrackTabAssociations();
  ContextualTask task(base::Uuid::GenerateRandomV4());
  existing_tasks_.insert(task.GetTaskId());

  tabs::TabInterface* restored_tab = CreateMockTab();
  SetTabRestoreData(restored_tab, task.GetTaskId(), GURL());
  coordinator_->OnTabAdded(*tab_list_, restored_tab, /*index=*/1);

  EXPECT_FALSE(tab_to_task_.contains(
      sessions::SessionTabHelper::IdForTab(restored_tab->GetContents())));
  EXPECT_FALSE(ContextualTasksTabRestoreData::FromWebContents(
      restored_tab->GetContents()));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseTaskPreservesTabWhenToolbarButtonPinned) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "no-entry-point"}}},
       {kEnableContextualTasksPinButtonInToolbar, {}},
       {kContextualTasksPinningV2, {}}},
      {});

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  ASSERT_TRUE(model);
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks,
                           /*should_pin=*/true);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(task));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task.GetTaskId()))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(task.GetTaskId(), active_tab_id))
      .Times(0);

  coordinator_->Close();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OpenInZeroState_PinnedToolbarButton) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableContextualTasksPinButtonInToolbar);

  // Pin the action in the PinnedToolbarActionsModel.
  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  ASSERT_TRUE(model);
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks,
                           /*should_pin=*/true);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));

  EXPECT_CALL(
      *mock_ui_service,
      SetInitialEntryPointForTask(
          task.GetTaskId(), omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON))
      .Times(1);

  coordinator_->OpenInZeroState();
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseLensSessionWhenShowingPanel) {
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  ASSERT_TRUE(active_tab);

  testing::NiceMock<lens::MockLensSearchController> mock_lens_controller(
      active_tab);

  EXPECT_CALL(mock_lens_controller, IsActive()).WillOnce(Return(true));
  EXPECT_CALL(
      mock_lens_controller,
      CloseLensSync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen))
      .Times(1);

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);
}
#endif

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_LensOverlay) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel with Lens Overlay entry point.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via user action (simulating UI transition
  // completion).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  // Verify that the Lens Overlay close metric was recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.LensOverlay", true, 1);

  // Verify that other close metrics were NOT recorded.
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_AiModeLinkClick) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel with transition_from_tab = true (representing AI Mode
  // link click).
  coordinator_->Show(
      /*transition_from_tab=*/true,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via user action (simulating UI transition
  // completion).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  // Verify that the AI Mode Link Click close metric was recorded.
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick", true, 1);

  // Verify that other close metrics were NOT recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_Other) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel with transition_from_tab = false and
  // non-Lens/non-composebox entry point.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via user action (simulating UI transition
  // completion).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  // Verify that the Other close metric was recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.Other", true, 1);

  // Verify that other close metrics were NOT recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AioToCobr"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close."
                   "EphemeralToolbarButton"));
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 0);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_AskGoogle) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel with an Ask Google entry point.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via user action (simulating UI transition
  // completion).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  // Verify that the Ask Google close metric was recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.AskGoogle", true, 1);

  // Verify that other close metrics were NOT recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AioToCobr"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_AioToCobr) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel with DESKTOP_CHROME_COBROWSE_AIO_LINK entry point.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_AIO_LINK);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 0);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via user action (simulating UI transition
  // completion).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  // Verify that the AioToCobr close metric and session duration were recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AioToCobr"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.AioToCobr", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 1);

  // Verify that other close metrics were NOT recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_AioToCobr_CustomOpenTime) {
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  base::TimeTicks open_time = base::TimeTicks::Now() - base::Seconds(3);
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_AIO_LINK,
      /*use_no_animation=*/false, open_time);

  coordinator_->Close();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       DestructorLogsMetrics_AioToCobr) {
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_AIO_LINK);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 0);

  // Resetting coordinator triggers the destructor which records session end
  // metrics.
  mock_panel_host_ = nullptr;
  coordinator_.reset();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CleanUpUnusedWebContentsLogsMetrics_AioToCobr) {
  base::HistogramTester histogram_tester;
  ClearCacheForTesting();

  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillRepeatedly(Return(std::vector<SessionID>{}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillRepeatedly(Return(std::nullopt));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/false);
  SetCacheItemEntrySourceForTesting(
      task_id, ContextualTasksPanelController::EntrySource::kAioToCobr,
      base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 0);

  // Clean up unused web contents (task has no associated tabs and is not
  // active).
  CleanUpUnusedWebContents();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.AioToCobr", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_EphemeralToolbarButton) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 0);

  coordinator_->Close();

  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close."
                   "EphemeralToolbarButton"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.EphemeralToolbarButton", true,
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 1);

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AioToCobr"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       DestructorLogsMetrics_EphemeralToolbarButton) {
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 0);

  mock_panel_host_ = nullptr;
  coordinator_.reset();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CleanUpUnusedWebContentsLogsMetrics_EphemeralToolbarButton) {
  base::HistogramTester histogram_tester;
  ClearCacheForTesting();

  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillRepeatedly(Return(std::vector<SessionID>{}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillRepeatedly(Return(std::nullopt));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/false);
  SetCacheItemEntrySourceForTesting(
      task_id,
      ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton,
      base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 0);

  CleanUpUnusedWebContents();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.EphemeralToolbarButton", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelLogsMetrics_PermanentToolbarButton_PinningV2) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(kContextualTasksPinningV2);

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Continuing an existing thread via the pinned toolbar button under Pinning
  // v2 must NOT log AEP (DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON), and
  // should instead pass UNKNOWN_AIM_ENTRY_POINT.
  EXPECT_CALL(*mock_ui_service,
              SetInitialEntryPointForTask(
                  task.GetTaskId(),
                  omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT))
      .Times(1);

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON);

  EXPECT_EQ(
      coordinator_->GetActiveEntrySource(),
      ContextualTasksPanelController::EntrySource::kPermanentToolbarButton);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 0);

  coordinator_->Close();

  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close."
                   "PermanentToolbarButton"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SidePanel.UserAction.Close.PermanentToolbarButton", true,
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 1);

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.AskGoogle"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close."
                   "EphemeralToolbarButton"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       DestructorLogsMetrics_PermanentToolbarButton_PinningV2) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(kContextualTasksPinningV2);

  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 0);

  mock_panel_host_ = nullptr;
  coordinator_.reset();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CleanUpUnusedWebContentsLogsMetrics_PermanentToolbarButton_PinningV2) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(kContextualTasksPinningV2);

  base::HistogramTester histogram_tester;
  ClearCacheForTesting();

  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillRepeatedly(Return(std::vector<SessionID>{}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillRepeatedly(Return(std::nullopt));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/false);
  SetCacheItemEntrySourceForTesting(
      task_id,
      ContextualTasksPanelController::EntrySource::kPermanentToolbarButton,
      base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 0);

  CleanUpUnusedWebContents();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Session.Duration.PermanentToolbarButton", 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelNoLogging_NonUserAction) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  // Show the side panel.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);

  // Call Close() programmatically (simulating framework close).
  coordinator_->Close();

  // Trigger the close event via a NON-user action (e.g. programmatic close due
  // to navigation or eligibility).
  coordinator_->OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kSystemAction);

  // Verify that NO close metrics were recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.LensOverlay"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.SidePanel.UserAction.Close.Other"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, OnTabRemoved_ActiveTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup: Create a tab with a task, and cache it with is_open = true.
  tabs::TabInterface* tab = tab_list_->GetActiveTab();
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  ON_CALL(*mock_controller_,
          GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(tab->GetContents())))
      .WillByDefault(Return(task));

  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);

  // Make this cached web contents the active one in the panel.
  UpdateWebContentsForActiveTab();
  ASSERT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(task_id));

  // Action: Remove the tab.
  coordinator_->OnTabRemoved(*tab_list_, tab, TabRemovedReason::kDeleted);

  // Verify: Recorded kActiveTab (0) and the correct user action.
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Tab.ClosedWithSidePanelOpenState",
      ContextualTasksTabCloseState::kActiveTab, 1);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          "ContextualTasks.Tab.UserAction.ClosedActiveTabWithSidePanelOpen"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.Tab.UserAction."
                   "ClosedBackgroundTabWithSidePanelOpen"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, OnTabRemoved_BackgroundTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup:
  // 1. Create Tab A (which will be backgrounded) with Task A, cached as open.
  tabs::TabInterface* tab_a = tab_list_->GetActiveTab();
  base::Uuid task_id_a = base::Uuid::GenerateRandomV4();
  ContextualTask task_a(task_id_a);
  ON_CALL(*mock_controller_,
          GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(tab_a->GetContents())))
      .WillByDefault(Return(task_a));
  CreateCachedWebContentsForTesting(task_id_a, /*is_open=*/true);

  // 2. Create Tab B (active) with Task B.
  tabs::TabInterface* tab_b = CreateMockTab();
  base::Uuid task_id_b = base::Uuid::GenerateRandomV4();
  ContextualTask task_b(task_id_b);
  ON_CALL(*mock_controller_,
          GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(tab_b->GetContents())))
      .WillByDefault(Return(task_b));
  CreateCachedWebContentsForTesting(task_id_b, /*is_open=*/false);

  // Make Tab B active in the tab list.
  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab_b));
  UpdateWebContentsForActiveTab();

  // Verify Tab A's task is in cache and has is_open = true, but is not active.
  ASSERT_NE(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(task_id_a));

  // Action: Remove Tab A (background tab).
  coordinator_->OnTabRemoved(*tab_list_, tab_a, TabRemovedReason::kDeleted);

  // Verify: Recorded kBackgroundTab (1) and the correct user
  // action.
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Tab.ClosedWithSidePanelOpenState",
      ContextualTasksTabCloseState::kBackgroundTab, 1);
  EXPECT_EQ(
      0,
      user_action_tester.GetActionCount(
          "ContextualTasks.Tab.UserAction.ClosedActiveTabWithSidePanelOpen"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.Tab.UserAction."
                   "ClosedBackgroundTabWithSidePanelOpen"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, OnTabRemoved_PanelClosed) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup: Create a tab with a task, but cached with is_open = false.
  tabs::TabInterface* tab = tab_list_->GetActiveTab();
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  ON_CALL(*mock_controller_,
          GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(tab->GetContents())))
      .WillByDefault(Return(task));

  CreateCachedWebContentsForTesting(task_id, /*is_open=*/false);

  // Action: Remove the tab.
  coordinator_->OnTabRemoved(*tab_list_, tab, TabRemovedReason::kDeleted);

  // Verify: No metrics recorded because the panel was closed.
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Tab.ClosedWithSidePanelOpenState", 0);
  EXPECT_EQ(
      0,
      user_action_tester.GetActionCount(
          "ContextualTasks.Tab.UserAction.ClosedActiveTabWithSidePanelOpen"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.Tab.UserAction."
                   "ClosedBackgroundTabWithSidePanelOpen"));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnActiveTabChanged_OpenFullTab) {
  // Test case 1: Switches to full tab (side panel closed) -> OpenFullTab is
  // recorded
  {
    base::UserActionTester user_action_tester;
    tabs::TabInterface* tab = CreateMockTab();
    content::WebContentsTester::For(tab->GetContents())
        ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
    coordinator_->OnActiveTabChanged(*tab_list_, tab);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "ContextualTasks.TabChange.UserAction.OpenFullTab"));
  }

  // Test case 2: Switches to full tab but side panel is open -> OpenFullTab is
  // not recorded
  {
    base::UserActionTester user_action_tester;
    ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
        .WillByDefault(Return(true));
    tabs::TabInterface* tab = CreateMockTab();
    content::WebContentsTester::For(tab->GetContents())
        ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
    coordinator_->OnActiveTabChanged(*tab_list_, tab);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "ContextualTasks.TabChange.UserAction.OpenFullTab"));
  }

  // Test case 3: Switches to non AIM tab -> OpenFullTab is not recorded
  {
    base::UserActionTester user_action_tester;
    ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
        .WillByDefault(Return(false));
    tabs::TabInterface* tab = CreateMockTab();
    content::WebContentsTester::For(tab->GetContents())
        ->NavigateAndCommit(GURL("https://google.com"));
    coordinator_->OnActiveTabChanged(*tab_list_, tab);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "ContextualTasks.TabChange.UserAction.OpenFullTab"));
  }
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnActiveTabChanged_LinkClickInTab) {
  base::UserActionTester user_action_tester;

  // 1. Setup: Start with contextual tasks tab active.
  tabs::TabInterface* ct_tab = CreateMockTab();
  content::WebContentsTester::For(ct_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));

  // Make ct_tab active.
  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(ct_tab));
  coordinator_->OnActiveTabChanged(*tab_list_, ct_tab);

  int initial_count = user_action_tester.GetActionCount(
      "ContextualTasks.TabChange.UserAction.OpenFullTab");
  EXPECT_EQ(initial_count, 1);

  // 2. Click link flow:
  // Create a new tab (simulating the link destination).
  tabs::TabInterface* new_tab = CreateMockTab();
  // Simulate CopyStateFrom (new_tab committed URL is
  // chrome://contextual-tasks).
  content::WebContentsTester::For(new_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));

  // Mock new_tab's opener to be ct_tab.
  ON_CALL(*tab_list_, GetOpenerForTab(new_tab->GetHandle()))
      .WillByDefault(Return(ct_tab));

  // Activate new_tab.
  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(new_tab));
  coordinator_->OnActiveTabChanged(*tab_list_, new_tab);

  // Detach ct_tab.
  coordinator_->OnTabRemoved(*tab_list_, ct_tab,
                             TabRemovedReason::kInsertedIntoSidePanel);

  // Show side panel.
  coordinator_->Show(true,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  // Verify: No new OpenFullTab was logged.
  int final_count = user_action_tester.GetActionCount(
      "ContextualTasks.TabChange.UserAction.OpenFullTab");
  EXPECT_EQ(final_count, 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, Show_StandardAnimation) {
  EXPECT_CALL(*mock_panel_host_,
              Show(ContextualTasksPanelHost::AnimationStyle::kStandard))
      .Times(1);
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Show_TransitionFromTabAnimation) {
  EXPECT_CALL(
      *mock_panel_host_,
      Show(ContextualTasksPanelHost::AnimationStyle::kTransitionFromTab))
      .Times(1);
  coordinator_->Show(
      /*transition_from_tab=*/true,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/false);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, Show_NoAnimation) {
  EXPECT_CALL(*mock_panel_host_,
              Show(ContextualTasksPanelHost::AnimationStyle::kNoAnimation))
      .Times(1);
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Show_NoAnimation_OverridesTransition) {
  EXPECT_CALL(*mock_panel_host_,
              Show(ContextualTasksPanelHost::AnimationStyle::kNoAnimation))
      .Times(1);
  coordinator_->Show(
      /*transition_from_tab=*/true,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       Show_LoadsInitialUrl_WithRearchitectureFlag) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));

  ContextualTask expected_task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(expected_task));

  GURL initial_url("https://www.google.com/search?udm=50&q=test");
  ON_CALL(*mock_ui_service, GetInitialUrlForTask(expected_task.GetTaskId()))
      .WillByDefault(Return(initial_url));

  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(1);

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);

  content::WebContents* cached_wc =
      GetWebContentsForTaskForTesting(expected_task.GetTaskId());
  ASSERT_TRUE(cached_wc);
  GURL expected_url(
      "https://www.google.com/search?udm=50&q=test&cs=0&gsc=2&hl=en");
  EXPECT_EQ(cached_wc->GetVisibleURL(), expected_url);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OpenInZeroState_LoadsDefaultAiPageUrl_WithRearchitectureFlag) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));

  ContextualTask new_task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(std::nullopt))
      .WillRepeatedly(Return(new_task));
  EXPECT_CALL(*mock_controller_, CreateTask()).WillOnce(Return(new_task));

  GURL default_ai_url("https://www.google.com/search?udm=50&aep=46");
  ON_CALL(*mock_ui_service, GetInitialUrlForTask(new_task.GetTaskId()))
      .WillByDefault(Return(std::nullopt));
  ON_CALL(*mock_ui_service, GetDefaultAiPageUrlForTask(new_task.GetTaskId()))
      .WillByDefault(Return(default_ai_url));

  EXPECT_CALL(
      *mock_ui_service,
      SetInitialEntryPointForTask(
          new_task.GetTaskId(),
          omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT))
      .Times(1);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(1);

  coordinator_->OpenInZeroState();

  content::WebContents* cached_wc =
      GetWebContentsForTaskForTesting(new_task.GetTaskId());
  ASSERT_TRUE(cached_wc);
  GURL expected_url(
      "https://www.google.com/search?udm=50&aep=46&cs=0&gsc=2&hl=en");
  EXPECT_EQ(cached_wc->GetVisibleURL(), expected_url);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       PanelThreadObserver_AttachedAndRemovedOnDetach_WithRearchitectureFlag) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));
  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));
  ON_CALL(*mock_ui_service, GetInitialUrlForTask(task.GetTaskId()))
      .WillByDefault(
          Return(GURL("https://www.google.com/search?udm=50&q=test")));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);

  content::WebContents* cached_wc =
      GetWebContentsForTaskForTesting(task.GetTaskId());
  ASSERT_TRUE(cached_wc);
  EXPECT_TRUE(ContextualTasksPanelThreadObserver::FromWebContents(cached_wc));

  // Moving the `WebContents` to a tab removes the observer.
  std::unique_ptr<content::WebContents> detached_wc =
      coordinator_->DetachWebContentsForTask(task.GetTaskId());
  ASSERT_EQ(detached_wc.get(), cached_wc);
  EXPECT_FALSE(
      ContextualTasksPanelThreadObserver::FromWebContents(detached_wc.get()));
}

// Post-rearchitecture, a panel `WebContents` reporting its own task change
// (see `ContextualTasksPanelThreadObserver`) is re-keyed, even if it isn't the
// active panel `WebContents`.
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnTaskChanged_RekeysReportingWebContents_WithRearchitectureFlag) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  base::Uuid active_task_id = base::Uuid::GenerateRandomV4();
  ON_CALL(*mock_controller_,
          GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(active_tab->GetContents())))
      .WillByDefault(Return(ContextualTask(active_task_id)));
  CreateCachedWebContentsForTesting(active_task_id, /*is_open=*/true);
  base::Uuid other_task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(other_task_id, /*is_open=*/false);
  UpdateWebContentsForActiveTab();

  content::WebContents* active_wc =
      GetWebContentsForTaskForTesting(active_task_id);
  content::WebContents* other_wc =
      GetWebContentsForTaskForTesting(other_task_id);
  ASSERT_TRUE(active_wc);
  ASSERT_TRUE(other_wc);
  ASSERT_EQ(coordinator_->GetActiveWebContents(), active_wc);

  base::Uuid new_task_id = base::Uuid::GenerateRandomV4();
  coordinator_->OnTaskChanged(other_wc, new_task_id);

  EXPECT_EQ(GetWebContentsForTaskForTesting(new_task_id), other_wc);
  EXPECT_EQ(GetWebContentsForTaskForTesting(active_task_id), active_wc);
  EXPECT_FALSE(GetWebContentsForTaskForTesting(other_task_id));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// The extensions menu dereferences extensions::TabHelper::FromWebContents()
// without a null check, so the side panel's WebContents must have one attached
// even though it is not a tab.
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       PanelWebContentsHasExtensionsTabHelper) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  ContextualTask expected_task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(expected_task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);

  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(expected_task.GetTaskId());
  ASSERT_TRUE(panel_contents);
  EXPECT_TRUE(extensions::TabHelper::FromWebContents(panel_contents));
  EXPECT_TRUE(
      permissions::PermissionRecoverySuccessRateTracker::FromWebContents(
          panel_contents));
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
      panel_contents->GetPrimaryMainFrame()));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnSeeExtensionsClicked_NoActiveContents) {
  TriggerOnSeeExtensionsClicked();
  EXPECT_EQ(GetExtensionsContainer(), nullptr);
}
#endif

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       TimeToFirstContentfulPaint_RecordedOncePerSession) {
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);

  content::WebContents* cached_wc =
      GetWebContentsForTaskForTesting(task.GetTaskId());
  ASSERT_TRUE(cached_wc);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", 0);

  // Trigger RecordTimeToFirstContentfulPaint with unrelated WebContents (should
  // be ignored).
  auto other_wc = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  coordinator_->RecordTimeToFirstContentfulPaint(other_wc.get());
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", 0);

  // Trigger RecordTimeToFirstContentfulPaint with the cached WebContents.
  coordinator_->RecordTimeToFirstContentfulPaint(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", 1);

  // Duplicate paint event in the same session should NOT record again.
  coordinator_->RecordTimeToFirstContentfulPaint(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", 1);

  // Calling Show() again starts a new session, resetting the recorded flag.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);

  coordinator_->RecordTimeToFirstContentfulPaint(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", 2);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       TimeToHandshakeComplete_RecordedOncePerSession) {
  base::HistogramTester histogram_tester;

  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);

  content::WebContents* cached_wc =
      GetWebContentsForTaskForTesting(task.GetTaskId());
  ASSERT_TRUE(cached_wc);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToHandshakeComplete", 0);

  // Trigger RecordTimeToHandshakeComplete with unrelated WebContents (should be
  // ignored).
  auto other_wc = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  coordinator_->RecordTimeToHandshakeComplete(other_wc.get());
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToHandshakeComplete", 0);

  // Trigger RecordTimeToHandshakeComplete with the cached WebContents.
  coordinator_->RecordTimeToHandshakeComplete(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToHandshakeComplete", 1);

  // Duplicate handshake complete event in the same session should NOT record
  // again.
  coordinator_->RecordTimeToHandshakeComplete(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToHandshakeComplete", 1);

  // Calling Show() again starts a new session, resetting the recorded flag.
  coordinator_->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);

  coordinator_->RecordTimeToHandshakeComplete(cached_wc);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.SidePanel.TimeToHandshakeComplete", 2);
}

class MockContextualTasksPermissionController
    : public ContextualTasksPermissionController {
 public:
  explicit MockContextualTasksPermissionController(
      BrowserWindowInterface* browser_window)
      : ContextualTasksPermissionController(browser_window) {}

  ~MockContextualTasksPermissionController() override = default;

  MOCK_METHOD(void, PushStateToWebUINow, (), (override));
};

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OnActiveTabChanged_PushesPermissionState) {
  auto permission_controller =
      std::make_unique<MockContextualTasksPermissionController>(
          browser_window_.get());
  auto* permission_controller_ptr = permission_controller.get();
  SetPermissionControllerForTesting(std::move(permission_controller));

  tabs::TabInterface* tab1 = CreateMockTab();
  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab1));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(testing::_))
      .WillRepeatedly(Return(task));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task_id))
      .WillRepeatedly(Return(std::vector<SessionID>{
          sessions::SessionTabHelper::IdForTab(tab1->GetContents())}));

  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);

  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(panel_contents);

  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab1));

  base::RunLoop run_loop;
  EXPECT_CALL(*permission_controller_ptr, PushStateToWebUINow())
      .WillOnce([&]() { run_loop.Quit(); });
  coordinator_->OnActiveTabChanged(*tab_list_, tab1);
  run_loop.Run();
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(
    ContextualTasksSidePanelCoordinatorTest,
    ShareTabsWithActiveOrNewThread_CreatesTaskAndAttachesActiveAndValidTabs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  tabs::TabInterface* active_tab = CreateMockTab();
  tabs::TabInterface* valid_tab = CreateMockTab();
  tabs::TabInterface* invalid_tab = CreateMockTab();
  content::WebContentsTester::For(active_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/active"));
  content::WebContentsTester::For(valid_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/shared"));
  content::WebContentsTester::For(invalid_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://version"));

  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(active_tab));
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));

  base::Uuid new_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask new_task(new_task_id);
  EXPECT_CALL(*mock_controller_, CreateTask()).WillOnce(Return(new_task));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(std::nullopt))
      .WillRepeatedly(Return(new_task));
  EXPECT_CALL(
      *mock_controller_,
      AssociateTabWithTask(new_task_id, sessions::SessionTabHelper::IdForTab(
                                            active_tab->GetContents())))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(
      *mock_controller_,
      AssociateTabWithTask(new_task_id, sessions::SessionTabHelper::IdForTab(
                                            valid_tab->GetContents())))
      .Times(0);
  EXPECT_CALL(
      *mock_controller_,
      AssociateTabWithTask(new_task_id, sessions::SessionTabHelper::IdForTab(
                                            invalid_tab->GetContents())))
      .Times(0);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(1);
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(active_tab->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(valid_tab->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(invalid_tab->GetHandle()))
      .Times(0);

  coordinator_->ShareTabsWithActiveOrNewThread(
      {valid_tab->GetHandle(), invalid_tab->GetHandle()});
}

TEST_F(
    ContextualTasksSidePanelCoordinatorTest,
    ShareTabsWithActiveOrNewThread_PanelAlreadyOpen_AttachesSharedTabToCurrentTask) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  tabs::TabInterface* active_tab = CreateMockTab();
  tabs::TabInterface* valid_tab = CreateMockTab();
  content::WebContentsTester::For(active_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/active"));
  content::WebContentsTester::For(valid_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/shared"));

  base::Uuid existing_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask existing_task(existing_task_id);
  CreateCachedWebContentsForTesting(existing_task_id, /*is_open=*/true);
  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(active_tab));
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(existing_task));

  EXPECT_CALL(*mock_controller_, CreateTask()).Times(0);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(0);
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(0);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(active_tab->GetHandle()))
      .Times(0);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(valid_tab->GetHandle()))
      .Times(1);

  coordinator_->ShareTabsWithActiveOrNewThread({valid_tab->GetHandle()});
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShareTabsWithActiveOrNewThread_IgnoresWhenNoValidTabs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  tabs::TabInterface* invalid_tab = CreateMockTab();
  content::WebContentsTester::For(invalid_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://version"));

  EXPECT_CALL(*mock_controller_, CreateTask()).Times(0);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(0);

  coordinator_->ShareTabsWithActiveOrNewThread({invalid_tab->GetHandle()});
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShareTabsWithExistingThread_SwitchesToThreadAndAttachesValidTabs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  tabs::TabInterface* active_tab = CreateMockTab();
  tabs::TabInterface* valid_tab = CreateMockTab();
  tabs::TabInterface* invalid_tab = CreateMockTab();
  content::WebContentsTester::For(active_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/active"));
  content::WebContentsTester::For(valid_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/shared"));
  content::WebContentsTester::For(invalid_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://version"));

  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(active_tab));

  base::Uuid previous_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask previous_task(previous_task_id);
  base::Uuid existing_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask existing_task(existing_task_id);
  CreateCachedWebContentsWithUrlForTesting(
      existing_task_id, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50&mtid=1&q=Existing+Thread"));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillOnce(Return(previous_task))
      .WillOnce(Return(previous_task))
      .WillRepeatedly(Return(existing_task));
  EXPECT_CALL(*mock_controller_,
              DisassociateTabFromTask(previous_task_id,
                                      sessions::SessionTabHelper::IdForTab(
                                          active_tab->GetContents())))
      .Times(1);
  EXPECT_CALL(*mock_controller_,
              AssociateTabWithTask(existing_task_id,
                                   sessions::SessionTabHelper::IdForTab(
                                       active_tab->GetContents())))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_controller_,
              AssociateTabWithTask(existing_task_id,
                                   sessions::SessionTabHelper::IdForTab(
                                       valid_tab->GetContents())))
      .Times(0);
  EXPECT_CALL(*mock_controller_,
              AssociateTabWithTask(existing_task_id,
                                   sessions::SessionTabHelper::IdForTab(
                                       invalid_tab->GetContents())))
      .Times(0);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(1);
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(active_tab->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(valid_tab->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(invalid_tab->GetHandle()))
      .Times(0);

  coordinator_->ShareTabsWithExistingThread(
      existing_task_id, {valid_tab->GetHandle(), invalid_tab->GetHandle()},
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  EXPECT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(existing_task_id));
  EXPECT_EQ(coordinator_->GetActiveEntrySource(),
            ContextualTasksPanelController::EntrySource::kOther);
}

TEST_F(
    ContextualTasksSidePanelCoordinatorTest,
    ShareTabsWithExistingThread_SameThreadPreservesLocalTabUnderlinesAndIgnoresInvalidInputs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  tabs::TabInterface* active_tab = CreateMockTab();
  tabs::TabInterface* valid_tab = CreateMockTab();
  tabs::TabInterface* invalid_tab = CreateMockTab();
  content::WebContentsTester::For(active_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/active"));
  content::WebContentsTester::For(valid_tab->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/shared"));
  content::WebContentsTester::For(invalid_tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://version"));

  ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(active_tab));

  base::Uuid existing_task_id = base::Uuid::GenerateRandomV4();
  ContextualTask existing_task(existing_task_id);
  CreateCachedWebContentsWithUrlForTesting(
      existing_task_id, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50&mtid=1&q=Existing+Thread"));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(existing_task));

  // Uncached `task_id` or empty valid tabs early-return without modifying
  // panel state or local underlines.
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(0);
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(0);
  coordinator_->ShareTabsWithExistingThread(
      base::Uuid::GenerateRandomV4(), {valid_tab->GetHandle()},
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  coordinator_->ShareTabsWithExistingThread(
      existing_task_id, {invalid_tab->GetHandle()},
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  // Sharing into the already-active thread (`switching_threads == false`)
  // preserves existing local tab underlines and keeps `active_tab` associated.
  EXPECT_CALL(*mock_controller_, DisassociateTabFromTask(_, _)).Times(0);
  EXPECT_CALL(mock_active_task_context_provider_, ClearAllLocalTabUnderlines())
      .Times(0);
  EXPECT_CALL(*mock_panel_host_, Show(_)).Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(active_tab->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              AddLocalTabUnderline(valid_tab->GetHandle()))
      .Times(1);

  coordinator_->ShareTabsWithExistingThread(
      existing_task_id, {valid_tab->GetHandle()},
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE);
  EXPECT_EQ(coordinator_->GetActiveEntrySource(),
            ContextualTasksPanelController::EntrySource::kAskGoogle);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ThemeChangesRefreshAllCachedSidePanelWebContents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kContextualTasks, kContextualTasksForceEntryPointEligibility,
       kContextualTasksSidePanelRearchitecture},
      {});

  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(theme_service, nullptr);
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  const base::Uuid active_task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid zero_state_task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid background_task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid waiting_task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid signin_task_id = base::Uuid::GenerateRandomV4();
  const base::Uuid captcha_task_id = base::Uuid::GenerateRandomV4();

  CreateCachedWebContentsForTesting(active_task_id, /*is_open=*/true);
  CreateCachedWebContentsForTesting(zero_state_task_id, /*is_open=*/false);
  CreateCachedWebContentsForTesting(background_task_id, /*is_open=*/false);
  CreateCachedWebContentsForTesting(waiting_task_id, /*is_open=*/true);
  CreateCachedWebContentsForTesting(signin_task_id, /*is_open=*/false);
  CreateCachedWebContentsForTesting(captcha_task_id, /*is_open=*/false);

  content::WebContents* active_wc =
      GetWebContentsForTaskForTesting(active_task_id);
  content::WebContents* zero_state_wc =
      GetWebContentsForTaskForTesting(zero_state_task_id);
  content::WebContents* background_wc =
      GetWebContentsForTaskForTesting(background_task_id);
  content::WebContents* waiting_wc =
      GetWebContentsForTaskForTesting(waiting_task_id);
  content::WebContents* signin_wc =
      GetWebContentsForTaskForTesting(signin_task_id);
  content::WebContents* captcha_wc =
      GetWebContentsForTaskForTesting(captcha_task_id);

  const GURL signin_url("https://login.corp.google.com/request");
  const GURL captcha_url(
      "https://www.google.com/sorry/index?continue=https://www.google.com/"
      "search");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      active_wc, ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
                     GURL("https://www.google.com/search?udm=50&q=hello"
                          "&mstk=turn1&mtid=thread1"),
                     active_wc));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      zero_state_wc,
      ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
          GURL("https://www.google.com/search?udm=50"), zero_state_wc));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      background_wc,
      ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
          GURL("https://www.google.co.uk/search?q=bg&gl=uk"), background_wc));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      waiting_wc, GURL("about:blank"));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(signin_wc,
                                                             signin_url);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(captcha_wc,
                                                             captcha_url);

  // When side panel rearchitecture is disabled, theme changes return early
  // without mutating state or reloading cached `WebContents`.
  {
    base::test::ScopedFeatureList disabled_rearch;
    disabled_rearch.InitAndDisableFeature(
        kContextualTasksSidePanelRearchitecture);
    theme_service->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kDark);
    EXPECT_EQ(active_wc->GetController().GetPendingEntry(), nullptr);
    theme_service->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kLight);
  }

  // Switching Light -> Dark immediately reloads both active AIM and cached
  // background ccTLD SRP `WebContents` with `cs=1` while keeping query params
  // intact, and skips `about:blank`, sign-in, and CAPTCHA URLs.
  theme_service->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);

  ASSERT_NE(active_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      active_wc->GetController().GetPendingEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  std::string cs_param;
  std::string q_param;
  std::string mstk_param;
  std::string mtid_param;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "cs", &cs_param));
  EXPECT_EQ("1", cs_param);
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "q", &q_param));
  EXPECT_EQ("hello", q_param);
  EXPECT_TRUE(net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "mstk",
                                         &mstk_param));
  EXPECT_EQ("turn1", mstk_param);
  EXPECT_TRUE(net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "mtid",
                                         &mtid_param));
  EXPECT_EQ("thread1", mtid_param);

  ASSERT_NE(zero_state_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_TRUE(net::GetValueForKeyInQuery(zero_state_wc->GetVisibleURL(), "cs",
                                         &cs_param));
  EXPECT_EQ("1", cs_param);

  ASSERT_NE(background_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_TRUE(net::GetValueForKeyInQuery(background_wc->GetVisibleURL(), "cs",
                                         &cs_param));
  EXPECT_EQ("1", cs_param);
  EXPECT_TRUE(net::GetValueForKeyInQuery(background_wc->GetVisibleURL(), "q",
                                         &q_param));
  EXPECT_EQ("bg", q_param);

  EXPECT_EQ(waiting_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_TRUE(waiting_wc->GetLastCommittedURL().IsAboutBlank());
  EXPECT_EQ(signin_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(signin_wc->GetLastCommittedURL(), signin_url);
  EXPECT_EQ(captcha_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(captcha_wc->GetLastCommittedURL(), captcha_url);
  EXPECT_EQ(active_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ(zero_state_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ(background_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ(waiting_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ(signin_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ(captcha_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);

  const int active_dark_pending_id =
      active_wc->GetController().GetPendingEntry()->GetUniqueID();
  const int zero_state_dark_pending_id =
      zero_state_wc->GetController().GetPendingEntry()->GetUniqueID();
  const int background_dark_pending_id =
      background_wc->GetController().GetPendingEntry()->GetUniqueID();

  // Switching Dark -> Light reloads with `cs=0`.
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  ASSERT_NE(active_wc->GetController().GetPendingEntry(), nullptr);
  const int active_light_pending_id =
      active_wc->GetController().GetPendingEntry()->GetUniqueID();
  EXPECT_NE(active_light_pending_id, active_dark_pending_id);
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "cs", &cs_param));
  EXPECT_EQ("0", cs_param);
  ASSERT_NE(zero_state_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_NE(zero_state_wc->GetController().GetPendingEntry()->GetUniqueID(),
            zero_state_dark_pending_id);
  EXPECT_TRUE(net::GetValueForKeyInQuery(zero_state_wc->GetVisibleURL(), "cs",
                                         &cs_param));
  EXPECT_EQ("0", cs_param);
  ASSERT_NE(background_wc->GetController().GetPendingEntry(), nullptr);
  const int background_light_pending_id =
      background_wc->GetController().GetPendingEntry()->GetUniqueID();
  EXPECT_NE(background_light_pending_id, background_dark_pending_id);
  EXPECT_TRUE(net::GetValueForKeyInQuery(background_wc->GetVisibleURL(), "cs",
                                         &cs_param));
  EXPECT_EQ("0", cs_param);
  EXPECT_EQ(active_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kLight);

  // Theme notifications that do not change effective dark mode must not reload.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  const ui::NativeTheme::PreferredColorScheme original_scheme =
      native_theme->preferred_color_scheme();
  absl::Cleanup restore_native_theme = [native_theme, original_scheme] {
    native_theme->set_preferred_color_scheme(original_scheme);
    native_theme->NotifyOnNativeThemeUpdated();
  };
  native_theme->set_preferred_color_scheme(
      ui::NativeTheme::PreferredColorScheme::kLight);
  native_theme->NotifyOnNativeThemeUpdated();
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kSystem);
  ASSERT_NE(active_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(active_wc->GetController().GetPendingEntry()->GetUniqueID(),
            active_light_pending_id);
  ASSERT_NE(background_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(background_wc->GetController().GetPendingEntry()->GetUniqueID(),
            background_light_pending_id);

  // Flipping OS `NativeTheme` from Light -> Dark under `kSystem` dispatches via
  // `native_theme_observation_` and reloads with `cs=1`.
  native_theme->set_preferred_color_scheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  native_theme->NotifyOnNativeThemeUpdated();

  ASSERT_NE(active_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_NE(active_wc->GetController().GetPendingEntry()->GetUniqueID(),
            active_light_pending_id);
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(active_wc->GetVisibleURL(), "cs", &cs_param));
  EXPECT_EQ("1", cs_param);
  ASSERT_NE(background_wc->GetController().GetPendingEntry(), nullptr);
  EXPECT_NE(background_wc->GetController().GetPendingEntry()->GetUniqueID(),
            background_light_pending_id);
  EXPECT_TRUE(net::GetValueForKeyInQuery(background_wc->GetVisibleURL(), "cs",
                                         &cs_param));
  EXPECT_EQ("1", cs_param);
  EXPECT_EQ(active_wc->GetOrCreateWebPreferences().preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
}
#endif

TEST_F(
    ContextualTasksSidePanelCoordinatorTest,
    GetRecentThreads_OrdersActiveFirstThenByRecencyAndSkipsZeroStateAndCapsAtFive) {
  base::TimeTicks base_ticks = base::TimeTicks::Now();

  // `about:blank`, non-AI Search SRPs, and zero-state AI URLs must be skipped
  // before querying task metadata.
  base::Uuid about_blank_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(about_blank_id, /*is_open=*/false,
                                           GURL("about:blank"));
  SetCacheItemLastActiveTimeForTesting(about_blank_id,
                                       base_ticks + base::Seconds(200));

  base::Uuid non_ai_srp_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      non_ai_srp_id, /*is_open=*/false,
      GURL("https://www.google.com/search?q=flowers&gsc=2"));
  SetCacheItemLastActiveTimeForTesting(non_ai_srp_id,
                                       base_ticks + base::Seconds(150));

  base::Uuid zero_state_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      zero_state_id, /*is_open=*/true,
      GURL("https://www.g.ai/search?udm=50&aep=1"));
  SetCacheItemLastActiveTimeForTesting(zero_state_id,
                                       base_ticks + base::Seconds(100));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(zero_state_id))
      .Times(0);

  // Five inactive non-zero-state threads with increasing recency.
  std::vector<base::Uuid> thread_ids;
  for (int i = 1; i <= 5; ++i) {
    base::Uuid id = base::Uuid::GenerateRandomV4();
    thread_ids.push_back(id);
    CreateCachedWebContentsWithUrlForTesting(
        id, /*is_open=*/false,
        GURL("https://www.google.com/search?udm=50&mtid=" +
             base::NumberToString(i) + "&q=Thread+" + base::NumberToString(i)));
    SetCacheItemLastActiveTimeForTesting(id, base_ticks + base::Seconds(i));
  }

  // Active thread in an open panel with older timestamp should appear first.
  base::Uuid active_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      active_id, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50&mtid=99&q=Active+Thread"));
  content::WebContents* active_wc = GetWebContentsForTaskForTesting(active_id);
  SetCacheItemLastActiveTimeForTesting(active_id,
                                       base_ticks - base::Seconds(10));
  mock_panel_host_->SetWebContents(active_wc);
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  std::vector<ContextualTasksPanelController::RecentThreadInfo> recent =
      coordinator_->GetRecentThreads();
  ASSERT_EQ(recent.size(), 5u);
  EXPECT_EQ(recent[0].task_id, active_id);
  EXPECT_EQ(recent[0].title, u"Active Thread");
  EXPECT_EQ(recent[1].task_id, thread_ids[4]);
  EXPECT_EQ(recent[1].title, u"Thread 5");
  EXPECT_EQ(recent[2].task_id, thread_ids[3]);
  EXPECT_EQ(recent[2].title, u"Thread 4");
  EXPECT_EQ(recent[3].task_id, thread_ids[2]);
  EXPECT_EQ(recent[3].title, u"Thread 3");
  EXPECT_EQ(recent[4].task_id, thread_ids[1]);
  EXPECT_EQ(recent[4].title, u"Thread 2");

  // Once the side panel is closed, `active_wc` is no longer open, so pure
  // recency order governs instead of pinning `active_id` first.
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  recent = coordinator_->GetRecentThreads();
  ASSERT_EQ(recent.size(), 5u);
  EXPECT_EQ(recent[0].task_id, thread_ids[4]);
  EXPECT_EQ(recent[4].task_id, thread_ids[0]);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       GetRecentThreads_ResolvesFallbackTitlesAndSkipsUntitled) {
  base::TimeTicks base_ticks = base::TimeTicks::Now();

  // 1. Thread without `q=` that resolves title via
  // `ContextualTask::GetThread()`.
  base::Uuid thread_struct_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      thread_struct_id, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50&mtid=1"));
  SetCacheItemLastActiveTimeForTesting(thread_struct_id,
                                       base_ticks + base::Seconds(3));
  SessionID tab_session_1 = SessionID::FromSerializedValue(101);
  ContextualTask task_with_thread(thread_struct_id);
  task_with_thread.AddThread(Thread(ThreadType::kAiMode, "server_id_1",
                                    "Thread Struct Title",
                                    /*last_turn_time_unix_epoch_millis=*/1));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(thread_struct_id))
      .WillOnce(Return(std::vector<SessionID>{tab_session_1}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(tab_session_1))
      .WillOnce(Return(task_with_thread));

  // 2. Thread without `q=` or `Thread` struct that resolves via
  // `task.GetTitle()`.
  base::Uuid task_title_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      task_title_id, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50&mtid=2"));
  SetCacheItemLastActiveTimeForTesting(task_title_id,
                                       base_ticks + base::Seconds(2));
  SessionID tab_session_2 = SessionID::FromSerializedValue(102);
  ContextualTask task_with_title(task_title_id);
  task_with_title.SetTitle("Task Fallback Title");
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task_title_id))
      .WillOnce(Return(std::vector<SessionID>{tab_session_2}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(tab_session_2))
      .WillOnce(Return(task_with_title));

  // 3. Zero-state AI URL and non-zero-state `mtid=` threads without `q=` or
  // `ContextualTask` title are skipped even when `WebContents` has a non-empty
  // page title.
  base::Uuid zero_state_titled_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      zero_state_titled_id, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50"));
  content::WebContentsTester::For(
      GetWebContentsForTaskForTesting(zero_state_titled_id))
      ->SetTitle(u"Google Search");
  SetCacheItemLastActiveTimeForTesting(zero_state_titled_id,
                                       base_ticks + base::Seconds(10));
  EXPECT_CALL(*mock_controller_,
              GetTabsAssociatedWithTask(zero_state_titled_id))
      .Times(0);

  base::Uuid untitled_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      untitled_id, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50&mtid=4"));
  content::WebContentsTester::For(GetWebContentsForTaskForTesting(untitled_id))
      ->SetTitle(u"WebContents Page Title");
  SetCacheItemLastActiveTimeForTesting(untitled_id,
                                       base_ticks + base::Seconds(11));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(untitled_id))
      .WillOnce(Return(std::vector<SessionID>{}));

  std::vector<ContextualTasksPanelController::RecentThreadInfo> recent =
      coordinator_->GetRecentThreads();
  ASSERT_EQ(recent.size(), 2u);
  EXPECT_EQ(recent[0].task_id, thread_struct_id);
  EXPECT_EQ(recent[0].title, u"Thread Struct Title");
  EXPECT_EQ(recent[1].task_id, task_title_id);
  EXPECT_EQ(recent[1].title, u"Task Fallback Title");
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       CloseSidePanelIgnoresInProgressPermissionRequest) {
  display::test::TestScreen test_screen(
      /*create_display=*/true, /*register_screen=*/!display::Screen::Get());
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksEphemeralBrandedEntryPoint,
        {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
      {});

  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  ASSERT_TRUE(active_tab);
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(task));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task_id))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);
  content::WebContents* panel_contents =
      GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(panel_contents);
  mock_panel_host_->SetWebContents(panel_contents);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      panel_contents, GURL(permissions::MockPermissionRequest::kDefaultOrigin));

  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(panel_contents);
  ASSERT_TRUE(permission_request_manager);
  permissions::MockPermissionPromptFactory prompt_factory(
      permission_request_manager);
  permission_request_manager->DocumentOnLoadCompletedInPrimaryMainFrame();

  auto request = std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kMicStream,
      permissions::PermissionRequestGestureType::GESTURE);
  permission_request_manager->AddRequest(panel_contents->GetPrimaryMainFrame(),
                                         std::move(request));
  prompt_factory.WaitForPermissionBubble();

  EXPECT_TRUE(permission_request_manager->IsRequestInProgress());
  EXPECT_TRUE(prompt_factory.is_visible());

  coordinator_->Close();

  EXPECT_FALSE(permission_request_manager->IsRequestInProgress());
  EXPECT_FALSE(prompt_factory.is_visible());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, IsShowingResults) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  EXPECT_FALSE(coordinator_->IsShowingResults());
  EXPECT_FALSE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_FALSE(IsShowingAimResults());

  // 1. Open panel in AIM zero state -> false.
  base::Uuid zero_state_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      zero_state_id, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50"));
  content::WebContents* zero_state_wc =
      GetWebContentsForTaskForTesting(zero_state_id);
  mock_panel_host_->SetWebContents(zero_state_wc);
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(ContextualTask(zero_state_id)));
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  EXPECT_FALSE(coordinator_->IsShowingResults());
  EXPECT_FALSE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_FALSE(IsShowingAimResults());

  // 1b. While a navigation to AIM results is still in progress (before commit),
  // HasResults() and IsShowingResults() are already true.
  auto pending_aim_nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com/search?udm=50&mtid=1&q=hello"),
      zero_state_wc);
  pending_aim_nav->Start();
  EXPECT_TRUE(coordinator_->IsShowingResults());
  EXPECT_TRUE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_TRUE(IsShowingAimResults());

  // 2. Open panel in non-zero-state AIM thread -> true (showing AIM results).
  base::Uuid active_thread_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      active_thread_id, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50&mtid=1&q=hello"));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(ContextualTask(active_thread_id)));
  mock_panel_host_->SetWebContents(
      GetWebContentsForTaskForTesting(active_thread_id));
  EXPECT_TRUE(coordinator_->IsShowingResults());
  EXPECT_TRUE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_TRUE(IsShowingAimResults());

  // 3. Open panel with a Lens SRP loaded -> true (showing Lens results).
  base::Uuid lens_srp_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      lens_srp_id, /*is_open=*/true,
      GURL("https://www.google.com/search?lns_mode=un&vsrid=123"));
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(ContextualTask(lens_srp_id)));
  mock_panel_host_->SetWebContents(
      GetWebContentsForTaskForTesting(lens_srp_id));
  EXPECT_TRUE(coordinator_->IsShowingResults());
  EXPECT_TRUE(coordinator_->HasResults());
  EXPECT_TRUE(IsShowingLensResults());
  EXPECT_FALSE(IsShowingAimResults());

  // 3b. Open panel from Lens Overlay entry point before inner frame commits ->
  // true (showing Lens results).
  base::Uuid lens_overlay_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      lens_overlay_id, /*is_open=*/true, GURL("chrome://contextual-tasks"));
  SetCacheItemEntrySourceForTesting(
      lens_overlay_id, ContextualTasksPanelController::EntrySource::kLensOverlay,
      base::TimeTicks::Now());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(ContextualTask(lens_overlay_id)));
  mock_panel_host_->SetWebContents(
      GetWebContentsForTaskForTesting(lens_overlay_id));
  EXPECT_TRUE(coordinator_->IsShowingResults());
  EXPECT_TRUE(coordinator_->HasResults());
  EXPECT_TRUE(IsShowingLensResults());
  EXPECT_FALSE(IsShowingAimResults());

  // 4. Closed panel with an active thread preserved on the active tab (even
  // when the cached WebContents URL is non-HTTP) -> IsShowingResults() is
  // false, while HasResults() and IsShowingAimResults() remain true.
  base::Uuid closed_thread_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      closed_thread_id, /*is_open=*/false, GURL("chrome://contextual-tasks"));
  ContextualTask closed_thread_task(closed_thread_id);
  closed_thread_task.AddThread(
      Thread(ThreadType::kAiMode, "thread_1", "Title", 1000));
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  mock_panel_host_->SetWebContents(nullptr);
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(closed_thread_task));
  EXPECT_FALSE(coordinator_->IsShowingResults());
  EXPECT_TRUE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_TRUE(IsShowingAimResults());

  // 5. Closed panel when the active tab has no associated task -> both false.
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(std::nullopt));
  EXPECT_FALSE(coordinator_->IsShowingResults());
  EXPECT_FALSE(coordinator_->HasResults());
  EXPECT_FALSE(IsShowingLensResults());
  EXPECT_FALSE(IsShowingAimResults());
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       UnshareTabs_RemovesSessionTokensAndLocalTabUnderlines) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksSidePanelRearchitecture, {}},
       {omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}}}},
      {});

  EXPECT_CALL(mock_active_task_context_provider_, RefreshContext()).Times(0);
  coordinator_->UnshareTabs({});
  testing::Mock::VerifyAndClearExpectations(
      &mock_active_task_context_provider_);

  tabs::TabInterface* tab1 = CreateMockTab();
  tabs::TabInterface* tab2 = CreateMockTab();
  tabs::TabInterface* tab3 = CreateMockTab();
  content::WebContentsTester::For(tab1->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/1"));
  content::WebContentsTester::For(tab2->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/2"));
  content::WebContentsTester::For(tab3->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/3"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab1), GetTitle())
      .WillByDefault(Return(u"Tab 1"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab2), GetTitle())
      .WillByDefault(Return(u"Tab 2"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab3), GetTitle())
      .WillByDefault(Return(u"Tab 3"));

  SessionID session_id1 =
      sessions::SessionTabHelper::IdForTab(tab1->GetContents());
  SessionID session_id2 =
      sessions::SessionTabHelper::IdForTab(tab2->GetContents());
  SessionID session_id3 =
      sessions::SessionTabHelper::IdForTab(tab3->GetContents());
  SetActiveTab(tab1);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ON_CALL(*mock_controller_, GetContextualTaskForTab(session_id1))
      .WillByDefault(Return(ContextualTask(task_id)));
  CreateCachedWebContentsWithUrlForTesting(
      task_id, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50&mtid=1&q=Thread"));
  content::WebContents* panel_wc = GetWebContentsForTaskForTesting(task_id);
  ASSERT_NE(panel_wc, nullptr);
  webui::SetBrowserWindowInterface(panel_wc, browser_window_.get());
  ASSERT_TRUE(UpdateWebContentsForActiveTab());
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  auto session_handle = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = session_handle.get();

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  base::UnguessableToken token3 = base::UnguessableToken::Create();

  NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  contextual_search::FileInfo submitted_file2;
  submitted_file2.file_token = token2;
  submitted_file2.tab_session_id = session_id2;
  submitted_file2.tab_url = GURL("https://example.com/2");
  submitted_file2.tab_title = "Tab 2";
  submitted_file2.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  submitted_file2.request_id = req2;

  contextual_search::FileInfo submitted_file3;
  submitted_file3.file_token = token3;
  submitted_file3.tab_session_id = session_id3;
  submitted_file3.tab_url = GURL("https://example.com/3");
  submitted_file3.tab_title = "Tab 3";
  submitted_file3.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId req3;
  req3.set_context_id(303);
  submitted_file3.request_id = req3;

  ON_CALL(*session_handle_ptr, GetController())
      .WillByDefault(Return(&mock_context_controller));
  ON_CALL(*session_handle_ptr, CreateContextToken())
      .WillByDefault([session_handle_ptr]() {
        auto token = base::UnguessableToken::Create();
        session_handle_ptr->GetUploadedContextTokensForTesting().push_back(
            token);
        return token;
      });
  ON_CALL(mock_context_controller, GetFileInfo(token2))
      .WillByDefault(Return(&submitted_file2));
  ON_CALL(mock_context_controller, GetFileInfo(token3))
      .WillByDefault(Return(&submitted_file3));

  session_handle_ptr->GetUploadedContextTokensForTesting() = {token1, token2,
                                                              token3};
  session_handle_ptr->SetSubmittedContextTokens({token2, token3});
  contextual_search::TabInfo restored_tab1;
  restored_tab1.tab_id = session_id1.id();
  restored_tab1.url = GURL("https://example.com/1");
  restored_tab1.title = "Tab 1";
  session_handle_ptr->SetRestoredTabs({restored_tab1});
  session_handle_ptr->AttachTabPendingUpload(
      token1, session_id1.id(), GURL("https://example.com/1"), "Tab 1");
  session_handle_ptr->AttachTabPendingUpload(
      token2, session_id2.id(), GURL("https://example.com/2"), "Tab 2");
  session_handle_ptr->AttachTabPendingUpload(
      token3, session_id3.id(), GURL("https://example.com/3"), "Tab 3");

  std::vector<contextual_search::FileInfo> submitted_files = {submitted_file2,
                                                              submitted_file3};
  ON_CALL(*session_handle_ptr, GetSubmittedContextFileInfos())
      .WillByDefault(Return(submitted_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(panel_wc)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(panel_wc);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);
  user_data->SetContextualTasksServiceForTesting(mock_controller_);
  user_data->RecordTabIdMapping(tab1->GetHandle().raw_value(),
                                session_id1.id());
  user_data->RecordTabIdMapping(tab2->GetHandle().raw_value(),
                                session_id2.id());
  user_data->RecordTabIdMapping(tab3->GetHandle().raw_value(),
                                session_id3.id());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  std::vector<int32_t> updated_tab_ids;
  NiceMock<MockContextualTasksExtensionPage> mock_ext_page;
  mojo::Remote<mojom::ExtensionPageHandler> ext_handler_remote;
  auto* ext_handler =
      ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
          panel_wc->GetPrimaryMainFrame());
  ASSERT_NE(ext_handler, nullptr);
  ON_CALL(mock_ext_page, OnTabContextUpdated(_, _))
      .WillByDefault(
          [&updated_tab_ids](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                             const std::vector<int32_t>&) {
            updated_tab_ids.clear();
            for (const auto& t : tabs) {
              updated_tab_ids.push_back(t->tab_id);
            }
          });
  ext_handler->CreateExtensionPageHandler(
      mock_ext_page.BindAndGetRemote(),
      ext_handler_remote.BindNewPipeAndPassReceiver());
  mock_ext_page.FlushForTesting();
#else
  const int dummy_handler_id = 0;
  user_data->RegisterExtensionFrame(&dummy_handler_id);
  user_data->UpdateExtensionFrameBound(
      &dummy_handler_id, /*is_page_bound=*/true, base::DoNothing(),
      base::DoNothing(), base::DoNothing());
#endif

  EXPECT_CALL(mock_active_task_context_provider_,
              RemoveLocalTabUnderline(tab1->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_,
              RemoveLocalTabUnderline(tab2->GetHandle()))
      .Times(1);
  EXPECT_CALL(mock_active_task_context_provider_, RefreshContext())
      .Times(testing::AtLeast(1));

  coordinator_->UnshareTabs(
      {tabs::TabHandle::Null(), tab1->GetHandle(), tab2->GetHandle()});
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  mock_ext_page.FlushForTesting();
  EXPECT_THAT(updated_tab_ids,
              testing::ElementsAre(tab3->GetHandle().raw_value()));
#endif

  const auto& remaining_attached =
      session_handle_ptr->GetTabContextState().attached;
  ASSERT_EQ(remaining_attached.size(), 1u);
  EXPECT_EQ(remaining_attached[0].tab_id, session_id3.id());
  EXPECT_THAT(session_handle_ptr->GetSubmittedContextTokens(),
              testing::ElementsAre(token3));
  EXPECT_TRUE(session_handle_ptr->IsTabDeselected(
      session_id1, GURL("https://example.com/1"), "Tab 1"));
  EXPECT_TRUE(session_handle_ptr->IsTabDeselected(
      session_id2, GURL("https://example.com/2"), "Tab 2"));
  EXPECT_FALSE(session_handle_ptr->IsTabDeselected(
      session_id3, GURL("https://example.com/3"), "Tab 3"));

#if !BUILDFLAG(IS_ANDROID)
  // Submitting a follow-up query while `tab1` is the active tab must not
  // recontextualize/re-upload the unshared tab and must report `202` in
  // `removed_contexts`.
  CountingTabContextualizationController tab1_contextualization_controller(
      tab1);
  int on_submit_responses = 0;
  std::vector<uint64_t> removed_context_ids;
  user_data->SetSearchMessageDispatcherForTesting(base::BindRepeating(
      [](int* count, std::vector<uint64_t>* removed_ids,
         const lens::ClientToSearchMessage& msg) {
        if (msg.has_on_submit_query_response()) {
          ++(*count);
          for (const auto& removed :
               msg.on_submit_query_response().removed_contexts()) {
            removed_ids->push_back(removed.request_id().context_id());
          }
        }
      },
      &on_submit_responses, &removed_context_ids));
  ON_CALL(*mock_controller_, GetContextForTask(task_id, _, _, _))
      .WillByDefault(
          [task_id, session_id1](
              const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            ContextualTask task_with_tab1(task_id);
            UrlResource resource(GURL("https://example.com/1"),
                                 ResourceType::kWebpage);
            resource.tab_id = session_id1;
            task_with_tab1.AddUrlResource(resource);
            std::move(callback).Run(
                std::make_unique<ContextualTaskContext>(task_with_tab1));
          });

  content::WebContentsTester::For(panel_wc)->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest();
  EXPECT_EQ(on_submit_responses, 1);
  EXPECT_THAT(removed_context_ids, testing::ElementsAre(202u));
  EXPECT_EQ(tab1_contextualization_controller.get_page_context_calls(), 0);

  // Re-sharing `tab1` clears deselected state and restores recontextualization.
  coordinator_->ShareTabsWithExistingThread(
      task_id, {tab1->GetHandle()},
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  EXPECT_EQ(tab1_contextualization_controller.get_page_context_calls(), 1);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  mock_ext_page.FlushForTesting();
#endif
  EXPECT_FALSE(session_handle_ptr->IsTabDeselected(
      session_id1, GURL("https://example.com/1"), "Tab 1"));

  const int calls_before_followup =
      tab1_contextualization_controller.get_page_context_calls();
  task_environment_.FastForwardBy(base::Milliseconds(1));
  content::WebContentsTester::For(panel_wc)->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest();
  EXPECT_EQ(on_submit_responses, 2);
  EXPECT_EQ(tab1_contextualization_controller.get_page_context_calls(),
            calls_before_followup + 1);
#endif

#if !(BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID))
  user_data->UnregisterExtensionFrame(&dummy_handler_id);
#endif
  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  webui::SetBrowserWindowInterface(panel_wc, nullptr);
  ON_CALL(*session_handle_ptr, GetController()).WillByDefault(Return(nullptr));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       UnshareTabs_RemovesTabFromEveryCachedThread) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksSidePanelRearchitecture, {}},
       {omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}}}},
      {});

  tabs::TabInterface* tab1 = CreateMockTab();
  tabs::TabInterface* tab2 = CreateMockTab();
  content::WebContentsTester::For(tab1->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/shared"));
  content::WebContentsTester::For(tab2->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/keep"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab1), GetTitle())
      .WillByDefault(Return(u"Shared Tab"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab2), GetTitle())
      .WillByDefault(Return(u"Keep Tab"));

  SessionID session_id1 =
      sessions::SessionTabHelper::IdForTab(tab1->GetContents());
  SessionID session_id2 =
      sessions::SessionTabHelper::IdForTab(tab2->GetContents());

  base::Uuid task_a = base::Uuid::GenerateRandomV4();
  base::Uuid task_b = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsWithUrlForTesting(
      task_a, /*is_open=*/true,
      GURL("https://www.google.com/search?udm=50&mtid=1&q=Thread+A"));
  CreateCachedWebContentsWithUrlForTesting(
      task_b, /*is_open=*/false,
      GURL("https://www.google.com/search?udm=50&mtid=2&q=Thread+B"));

  NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  auto session_a = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto session_b = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_a_ptr = session_a.get();
  auto* session_b_ptr = session_b.get();
  ON_CALL(*session_a_ptr, GetController())
      .WillByDefault(Return(&mock_context_controller));
  ON_CALL(*session_b_ptr, GetController())
      .WillByDefault(Return(&mock_context_controller));

  base::UnguessableToken token_a1 = base::UnguessableToken::Create();
  base::UnguessableToken token_b1 = base::UnguessableToken::Create();
  base::UnguessableToken token_b2 = base::UnguessableToken::Create();
  lens::LensOverlayRequestId req_a1;
  req_a1.set_context_id(11);
  lens::LensOverlayRequestId req_b1;
  req_b1.set_context_id(22);

  session_a_ptr->GetUploadedContextTokensForTesting() = {token_a1};
  session_a_ptr->AttachTabPendingUpload(token_a1, session_id1.id(),
                                        GURL("https://example.com/shared"),
                                        "Shared Tab");
  contextual_search::FileInfo file_a1;
  file_a1.file_token = token_a1;
  file_a1.tab_session_id = session_id1;
  file_a1.tab_url = GURL("https://example.com/shared");
  file_a1.tab_title = "Shared Tab";
  file_a1.request_id = req_a1;
  ON_CALL(mock_context_controller, GetFileInfo(token_a1))
      .WillByDefault(Return(&file_a1));
  ON_CALL(*session_a_ptr, GetSubmittedContextFileInfos())
      .WillByDefault(Return(std::vector<contextual_search::FileInfo>{file_a1}));

  session_b_ptr->GetUploadedContextTokensForTesting() = {token_b1, token_b2};
  session_b_ptr->AttachTabPendingUpload(token_b1, session_id1.id(),
                                        GURL("https://example.com/shared"),
                                        "Shared Tab");
  session_b_ptr->AttachTabPendingUpload(
      token_b2, session_id2.id(), GURL("https://example.com/keep"), "Keep Tab");
  contextual_search::FileInfo file_b1;
  file_b1.file_token = token_b1;
  file_b1.tab_session_id = session_id1;
  file_b1.tab_url = GURL("https://example.com/shared");
  file_b1.tab_title = "Shared Tab";
  file_b1.request_id = req_b1;
  ON_CALL(mock_context_controller, GetFileInfo(token_b1))
      .WillByDefault(Return(&file_b1));
  ON_CALL(*session_b_ptr, GetSubmittedContextFileInfos())
      .WillByDefault(Return(std::vector<contextual_search::FileInfo>{file_b1}));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      GetWebContentsForTaskForTesting(task_a))
      ->SetTaskSession(task_a, std::move(session_a),
                       /*input_state_model=*/nullptr);
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      GetWebContentsForTaskForTesting(task_b))
      ->SetTaskSession(task_b, std::move(session_b),
                       /*input_state_model=*/nullptr);

  coordinator_->UnshareTabs({tab1->GetHandle()});

  EXPECT_TRUE(session_a_ptr->GetTabContextState().attached.empty());
  const auto removed_a = session_a_ptr->TakeRemovedContexts();
  ASSERT_EQ(removed_a.size(), 1u);
  EXPECT_EQ(removed_a[0].context_id(), 11);
  EXPECT_TRUE(session_a_ptr->IsTabDeselected(
      session_id1, GURL("https://example.com/shared"), "Shared Tab"));

  ASSERT_EQ(session_b_ptr->GetTabContextState().attached.size(), 1u);
  EXPECT_EQ(session_b_ptr->GetTabContextState().attached[0].tab_id,
            session_id2.id());
  const auto removed_b = session_b_ptr->TakeRemovedContexts();
  ASSERT_EQ(removed_b.size(), 1u);
  EXPECT_EQ(removed_b[0].context_id(), 22);
  EXPECT_TRUE(session_b_ptr->IsTabDeselected(
      session_id1, GURL("https://example.com/shared"), "Shared Tab"));
  ON_CALL(*session_a_ptr, GetController()).WillByDefault(Return(nullptr));
  ON_CALL(*session_b_ptr, GetController()).WillByDefault(Return(nullptr));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       UnshareTabs_RevokesFullTabAiModeSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasks, {}},
       {kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksSidePanelRearchitecture, {}},
       {omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}}}},
      {});

  tabs::TabInterface* ai_tab = CreateMockTab();
  tabs::TabInterface* tab1 = CreateMockTab();
  tabs::TabInterface* tab2 = CreateMockTab();
  content::WebContentsTester::For(ai_tab->GetContents())
      ->NavigateAndCommit(
          GURL("https://www.google.com/search?udm=50&mtid=9&q=Full+Tab+AI"));
  content::WebContentsTester::For(tab1->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/1"));
  content::WebContentsTester::For(tab2->GetContents())
      ->NavigateAndCommit(GURL("https://example.com/2"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab1), GetTitle())
      .WillByDefault(Return(u"Tab 1"));
  ON_CALL(*static_cast<tabs::MockTabInterface*>(tab2), GetTitle())
      .WillByDefault(Return(u"Tab 2"));

  SessionID session_id1 =
      sessions::SessionTabHelper::IdForTab(tab1->GetContents());
  SessionID session_id2 =
      sessions::SessionTabHelper::IdForTab(tab2->GetContents());

  base::Uuid full_tab_task_id = base::Uuid::GenerateRandomV4();
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));
  webui::SetBrowserWindowInterface(ai_tab->GetContents(),
                                   browser_window_.get());
  auto* ai_user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          ai_tab->GetContents());
  ai_user_data->SetTaskId(full_tab_task_id);
  ai_user_data->RecordTabIdMapping(tab1->GetHandle().raw_value(),
                                   session_id1.id());
  ai_user_data->RecordTabIdMapping(tab2->GetHandle().raw_value(),
                                   session_id2.id());

  NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  base::UnguessableToken turn1_token1 = base::UnguessableToken::Create();
  contextual_search::FileInfo controller_file1;
  controller_file1.file_token = turn1_token1;
  controller_file1.tab_session_id = session_id1;
  controller_file1.tab_url = GURL("https://example.com/1");
  controller_file1.tab_title = "Tab 1";
  lens::LensOverlayRequestId req1;
  req1.set_context_id(501);
  controller_file1.request_id = req1;

  base::UnguessableToken turn1_token2 = base::UnguessableToken::Create();
  contextual_search::FileInfo controller_file2;
  controller_file2.file_token = turn1_token2;
  controller_file2.tab_session_id = session_id2;
  controller_file2.tab_url = GURL("https://example.com/2");
  controller_file2.tab_title = "Tab 2";
  lens::LensOverlayRequestId req2;
  req2.set_context_id(502);
  controller_file2.request_id = req2;

  ON_CALL(mock_context_controller, GetFileInfo(turn1_token1))
      .WillByDefault(Return(&controller_file1));
  ON_CALL(mock_context_controller, GetFileInfo(turn1_token2))
      .WillByDefault(Return(&controller_file2));
  ON_CALL(mock_context_controller, DeleteFile(_)).WillByDefault(Return(true));

  auto full_tab_session = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* full_tab_session_ptr = full_tab_session.get();
  ON_CALL(*full_tab_session_ptr, GetController())
      .WillByDefault(Return(&mock_context_controller));
  ON_CALL(*full_tab_session_ptr, GetSubmittedContextFileInfos())
      .WillByDefault(Return(std::vector<contextual_search::FileInfo>{
          controller_file1, controller_file2}));

  contextual_search::TabInfo restored_tab1;
  restored_tab1.tab_id = session_id1.id();
  restored_tab1.url = GURL("https://example.com/1");
  restored_tab1.title = "Tab 1";

  contextual_search::TabInfo restored_tab2;
  restored_tab2.tab_id = session_id2.id();
  restored_tab2.url = GURL("https://example.com/2");
  restored_tab2.title = "Tab 2";

  full_tab_session_ptr->SetRestoredTabs({restored_tab1, restored_tab2});
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      ai_tab->GetContents())
      ->SetTaskSession(full_tab_task_id, std::move(full_tab_session),
                       /*input_state_model=*/nullptr);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  std::vector<int32_t> full_tab_ext_ids;
  NiceMock<MockContextualTasksExtensionPage> full_tab_ext_page;
  mojo::Remote<mojom::ExtensionPageHandler> full_tab_ext_remote;
  auto* full_tab_ext_handler =
      ContextualTasksExtensionHandler::GetOrCreateForCurrentDocument(
          ai_tab->GetContents()->GetPrimaryMainFrame());
  ON_CALL(full_tab_ext_page, OnTabContextUpdated(_, _))
      .WillByDefault(
          [&full_tab_ext_ids](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                              const std::vector<int32_t>&) {
            full_tab_ext_ids.clear();
            for (const auto& t : tabs) {
              full_tab_ext_ids.push_back(t->tab_id);
            }
          });
  full_tab_ext_handler->CreateExtensionPageHandler(
      full_tab_ext_page.BindAndGetRemote(),
      full_tab_ext_remote.BindNewPipeAndPassReceiver());
  full_tab_ext_page.FlushForTesting();
#endif

  SetActiveTab(ai_tab);
  coordinator_->UnshareTabs({tab1->GetHandle()});
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  full_tab_ext_page.FlushForTesting();
  EXPECT_THAT(full_tab_ext_ids,
              testing::ElementsAre(tab2->GetHandle().raw_value()));
#endif
  const auto removed_turn1 = full_tab_session_ptr->TakeRemovedContexts();
  ASSERT_EQ(removed_turn1.size(), 1u);
  EXPECT_EQ(removed_turn1[0].context_id(), 501);

  SetActiveTab(tab2);
  coordinator_->UnshareTabs({tab2->GetHandle()});
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  full_tab_ext_page.FlushForTesting();
  EXPECT_TRUE(full_tab_ext_ids.empty());
#endif
  const auto removed_turn2 = full_tab_session_ptr->TakeRemovedContexts();
  ASSERT_EQ(removed_turn2.size(), 1u);
  EXPECT_EQ(removed_turn2[0].context_id(), 502);

  webui::SetBrowserWindowInterface(ai_tab->GetContents(), nullptr);
  ON_CALL(*full_tab_session_ptr, GetController())
      .WillByDefault(Return(nullptr));
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       NavigationMetrics_RecordsResponseCodesAndNetError) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  base::HistogramTester histogram_tester;
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);
  content::WebContents* web_contents = GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(web_contents);

  // 1. Successful commit with HTTP 200 on non-zero-state URL.
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n"));
  sim->Commit();

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Navigation.HttpResponseCode", 200, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Navigation.HttpResponseCode.ExcludeZeroStateLoads", 200,
      1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Navigation.NetErrorCode",
                                      0, 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       NavigationMetrics_ZeroStateExcludeZeroStateLoads) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  base::HistogramTester histogram_tester;
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);
  content::WebContents* web_contents = GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(web_contents);

  // Navigation to zero-state URL (https://g.ai/).
  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://g.ai/"), web_contents->GetPrimaryMainFrame());
  sim->SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n"));
  sim->Commit();

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Navigation.HttpResponseCode", 200, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Navigation.HttpResponseCode.ExcludeZeroStateLoads", 0);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Navigation.NetErrorCode",
                                      0, 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       NavigationMetrics_NetErrorCodeOnFailure) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  base::HistogramTester histogram_tester;
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);
  content::WebContents* web_contents = GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(web_contents);

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->Fail(net::ERR_TIMED_OUT);
  sim->CommitErrorPage();

  histogram_tester.ExpectBucketCount("ContextualTasks.Navigation.NetErrorCode",
                                     std::abs(net::ERR_TIMED_OUT), 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Navigation.HttpResponseCode", 0);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       NavigationMetrics_RecordedAfterTransferWebContentsFromTab) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kContextualTasksSidePanelRearchitecture);

  base::HistogramTester histogram_tester;
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  // Create an existing cached WebContents so TransferWebContentsFromTab
  // replaces it.
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);

  auto transferred_wc = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  content::WebContents* transferred_wc_ptr = transferred_wc.get();
  coordinator_->TransferWebContentsFromTab(task_id, std::move(transferred_wc));

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      transferred_wc_ptr->GetPrimaryMainFrame());
  sim->SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n"));
  sim->Commit();

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Navigation.HttpResponseCode", 200, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.Navigation.HttpResponseCode.ExcludeZeroStateLoads", 200,
      1);
  histogram_tester.ExpectUniqueSample("ContextualTasks.Navigation.NetErrorCode",
                                      0, 1);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       NavigationMetrics_NotRecordedWhenRearchitectureDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      kContextualTasksSidePanelRearchitecture);

  base::HistogramTester histogram_tester;
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id, /*is_open=*/true);
  content::WebContents* web_contents = GetWebContentsForTaskForTesting(task_id);
  ASSERT_TRUE(web_contents);

  auto sim = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search?q=test"),
      web_contents->GetPrimaryMainFrame());
  sim->SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n"));
  sim->Commit();

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Navigation.HttpResponseCode", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.Navigation.HttpResponseCode.ExcludeZeroStateLoads", 0);
  histogram_tester.ExpectTotalCount("ContextualTasks.Navigation.NetErrorCode",
                                    0);
}

}  // namespace contextual_tasks

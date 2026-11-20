// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockUiDelegate : public TabUnderlineController::UiDelegate {
 public:
  MockUiDelegate() {
    ON_CALL(*this, Show()).WillByDefault([this]() { is_showing_ = true; });
    ON_CALL(*this, StopShowing()).WillByDefault([this]() {
      is_showing_ = false;
    });
    ON_CALL(*this, StartRampingDown()).WillByDefault([this]() {
      is_showing_ = false;
    });
    ON_CALL(*this, IsShowing()).WillByDefault([this]() { return is_showing_; });
    ON_CALL(*this, CanShow()).WillByDefault([this]() { return can_show_; });
  }

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, StopShowing, (), (override));
  MOCK_METHOD(void, ResetAnimationCycle, (), (override));
  MOCK_METHOD(void, StartRampingDown, (), (override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(bool, CanShow, (), (const, override));

  void set_can_show(bool can_show) { can_show_ = can_show; }

 private:
  bool is_showing_ = false;
  bool can_show_ = true;
};

class FakeActiveTaskContextProvider
    : public contextual_tasks::ActiveTaskContextProvider {
 public:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  void SetContextualTasksPanelController(
      contextual_tasks::ContextualTasksPanelController* controller) override {}
  void AddLocalTabUnderline(tabs::TabHandle tab_handle) override {}
  void RemoveLocalTabUnderline(tabs::TabHandle tab_handle) override {}
  void ClearAllLocalTabUnderlines() override {}
  void RefreshContext() override {}

  void NotifyObservers(const std::set<tabs::TabHandle>& context_tabs) {
    for (auto& observer : observers_) {
      observer.OnContextTabsChanged(context_tabs);
    }
  }

 private:
  base::ObserverList<Observer> observers_;
};

class FakeGlicSharingManager : public GlicSharingManagerInternal {
 public:
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override {
    return {};
  }
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override {
    return {};
  }
  FocusedTabData GetFocusedTabData() override {
    return FocusedTabData("no focus", nullptr);
  }
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override {
    return {};
  }
  BrowserWindowInterface* GetFocusedBrowser() const override { return nullptr; }
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override {
    return {};
  }
  base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback) override {
    return {};
  }
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) override {
    return pinned_tabs_callbacks_.Add(std::move(callback));
  }
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) override {
    return {};
  }
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger) override {
    return false;
  }
  void SetPinTrigger(tabs::TabHandle tab_handle,
                     GlicPinTrigger trigger) override {}
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                 GlicUnpinTrigger trigger) override {
    return false;
  }
  void UnpinAllTabs(GlicUnpinTrigger trigger) override {}
  int32_t GetMaxPinnedTabs() const override { return 0; }
  int32_t GetNumPinnedTabs() const override { return 0; }
  int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) override { return 0; }
  std::vector<raw_ptr<tabs::TabInterface>> GetPinnedTabs() const override {
    return pinned_tabs_;
  }
  bool IsTabPinned(tabs::TabHandle tab_handle) const override {
    return pinned_handles_.contains(tab_handle);
  }
  bool IsTabShared(tabs::TabInterface* tab) const override { return false; }
  bool IsTabFocused(tabs::TabHandle tab_handle) const override { return false; }
  std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) override {
    return std::nullopt;
  }
  std::optional<GlicGetContextError> CheckPreliminaryContextSharingEligibility(
      tabs::TabHandle tab_handle) const override {
    return std::nullopt;
  }
  void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::TabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) override {}
  void GetContextForActorFromTab(
      tabs::TabHandle tab_handle,
      const mojom::TabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) override {}
  void GetImageBytes(
      tabs::TabHandle tab_handle,
      const std::string& document_id,
      int32_t dom_node_id,
      base::OnceCallback<void(GlicGetImageBytesResult)> callback) override {}
  void OnConversationTurnSubmitted() override {}
  base::WeakPtr<GlicSharingManagerInternal> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetPinnedTabs(std::vector<raw_ptr<tabs::TabInterface>> tabs) {
    pinned_handles_.clear();
    for (tabs::TabInterface* tab : tabs) {
      pinned_handles_.insert(tab->GetHandle());
    }
    pinned_tabs_ = std::move(tabs);
    pinned_tabs_callbacks_.Notify(pinned_tabs_);
  }

 private:
  base::RepeatingCallbackList<void(
      const std::vector<raw_ptr<tabs::TabInterface>>&)>
      pinned_tabs_callbacks_;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs_;
  std::set<tabs::TabHandle> pinned_handles_;
  base::WeakPtrFactory<FakeGlicSharingManager> weak_ptr_factory_{this};
};

class MockGlicKeyedServiceForUnderlineTest : public MockGlicKeyedService {
 public:
  MockGlicKeyedServiceForUnderlineTest(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service)
      : MockGlicKeyedService(browser_context,
                             identity_manager,
                             profile_manager,
                             glic_profile_manager,
                             contextual_cueing_service,
                             actor_keyed_service) {
    ON_CALL(mock_coordinator(), active_instance_sharing_manager())
        .WillByDefault(ReturnRef(sharing_manager_));
  }

  FakeGlicSharingManager& sharing_manager() { return sharing_manager_; }

 private:
  FakeGlicSharingManager sharing_manager_;
};

class TabUnderlineControllerTest : public testing::Test {
 public:
  void SetUp() override {
    GlicEnabling::SetSystemRequirementMetForTesting(true);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_tasks::kContextualTasks, {}},
            {dictation::kDictation, {{"use_component_extension", "false"}}},
            {features::kGlic, {}},
            {features::kGlicRollout, {}},
        },
        {features::kGlicShowForSignedOut});

    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        dictation::DictationKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<dictation::MockDictationKeyedService>(
              Profile::FromBrowserContext(context));
        }));
    factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            &TabUnderlineControllerTest::BuildMockGlicKeyedService,
            base::Unretained(this)));

    profile_ = testing_profile_manager->CreateTestingProfile(
        "profile", std::move(factories));
    glic_test_env_.SetupProfile(profile_);

    profile_->GetPrefs()->SetBoolean(::prefs::kPrefDictationOnboardingCompleted,
                                     true);

    dictation_service_ = static_cast<dictation::MockDictationKeyedService*>(
        dictation::DictationKeyedService::Get(profile_));
    ASSERT_TRUE(dictation_service_);

    glic_service_ = static_cast<MockGlicKeyedServiceForUnderlineTest*>(
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                     /*create=*/true));
    ASSERT_TRUE(glic_service_);

    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile()).WillByDefault(Return(profile_));
    ON_CALL(testing::Const(*browser_window_), GetProfile())
        .WillByDefault(Return(profile_));
    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(window_user_data_host_));
    ON_CALL(testing::Const(*browser_window_), GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(window_user_data_host_));

    context_provider_registration_ = std::make_unique<
        ui::ScopedUnownedUserData<contextual_tasks::ActiveTaskContextProvider>>(
        window_user_data_host_, context_provider_);

    tab_ = CreateMockTab(&web_contents_);
    other_tab_ = CreateMockTab(&other_web_contents_);

    controller_ = std::make_unique<TabUnderlineController>(tab_->GetHandle());
  }

  void TearDown() override {
    if (auto* session_controller = dictation_service_->session_controller()) {
      session_controller->EndSessionSynchronously();
    }
    glic_service_->sharing_manager().SetPinnedTabs({});
    controller_.reset();
    tab_.reset();
    other_tab_.reset();
    web_contents_.reset();
    other_web_contents_.reset();
    context_provider_registration_.reset();
    browser_window_.reset();
    dictation_service_ = nullptr;
    glic_service_ = nullptr;
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
    scoped_feature_list_.Reset();
  }

 protected:
  std::unique_ptr<KeyedService> BuildMockGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockGlicKeyedServiceForUnderlineTest>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr,
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
  }

  std::unique_ptr<tabs::MockTabInterface> CreateMockTab(
      std::unique_ptr<content::WebContents>* out_web_contents) {
    *out_web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    auto tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab, GetProfile()).WillByDefault(Return(profile_));
    ON_CALL(*tab, GetBrowserWindowInterface())
        .WillByDefault(Return(browser_window_.get()));
    ON_CALL(*tab, GetContents()).WillByDefault(Return(out_web_contents->get()));
    return tab;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  GlicUnitTestEnvironment glic_test_env_;

#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  GlicProfileManager glic_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<dictation::MockDictationKeyedService> dictation_service_ = nullptr;
  raw_ptr<MockGlicKeyedServiceForUnderlineTest> glic_service_ = nullptr;

  ui::UnownedUserDataHost window_user_data_host_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  FakeActiveTaskContextProvider context_provider_;
  std::unique_ptr<
      ui::ScopedUnownedUserData<contextual_tasks::ActiveTaskContextProvider>>
      context_provider_registration_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::WebContents> other_web_contents_;
  std::unique_ptr<tabs::MockTabInterface> tab_;
  std::unique_ptr<tabs::MockTabInterface> other_tab_;

  NiceMock<MockUiDelegate> ui_delegate_;
  std::unique_ptr<TabUnderlineController> controller_;
};

TEST_F(TabUnderlineControllerTest,
       RepeatedNoOpContextTabsChangedDoesNotStopShowing) {
  EXPECT_CALL(ui_delegate_, Show()).Times(0);
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(0);
  EXPECT_CALL(ui_delegate_, ResetAnimationCycle()).Times(0);

  controller_->Initialize(&ui_delegate_);
  controller_->OnUiReady();

  // Repeated broadcasts without this tab in context must be no-ops.
  context_provider_.NotifyObservers({});
  context_provider_.NotifyObservers({other_tab_->GetHandle()});
  context_provider_.NotifyObservers({});
}

TEST_F(TabUnderlineControllerTest, ContextualTasksShowAndHideTransitions) {
  controller_->Initialize(&ui_delegate_);
  controller_->OnUiReady();

  // Adding this tab to context shows the underline.
  {
    testing::InSequence seq;
    EXPECT_CALL(ui_delegate_, StopShowing()).Times(1);
    EXPECT_CALL(ui_delegate_, Show()).Times(1);
  }
  context_provider_.NotifyObservers({tab_->GetHandle()});
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Removing this tab from context hides the underline once.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(1);
  context_provider_.NotifyObservers({});
  EXPECT_FALSE(ui_delegate_.IsShowing());

  // Subsequent removals while already hidden must be no-ops.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  context_provider_.NotifyObservers({});
  context_provider_.NotifyObservers({other_tab_->GetHandle()});
}

TEST_F(TabUnderlineControllerTest, DictationNoOpAndTransitions) {
  // Initialize subscribes to DictationKeyedService, which immediately fires
  // OnDictationTabChanged(nullptr); this must not call StopShowing().
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(0);
  controller_->Initialize(&ui_delegate_);
  controller_->OnUiReady();

  // Starting and ending dictation on a different tab must also be a no-op.
  dictation_service_->StartSessionForTesting(
      *other_tab_, dictation::TargetDetails(),
      dictation::DictationSessionEntryPoint::kContextMenu);
  dictation_service_->session_controller()->EndSessionSynchronously();
  testing::Mock::VerifyAndClearExpectations(&ui_delegate_);

  // Starting dictation on this tab shows the underline.
  {
    testing::InSequence seq;
    EXPECT_CALL(ui_delegate_, StopShowing()).Times(1);
    EXPECT_CALL(ui_delegate_, Show()).Times(1);
  }
  dictation_service_->StartSessionForTesting(
      *tab_, dictation::TargetDetails(),
      dictation::DictationSessionEntryPoint::kContextMenu);
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Ending dictation hides the underline once.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(1);
  dictation_service_->session_controller()->EndSessionSynchronously();
  EXPECT_FALSE(ui_delegate_.IsShowing());

  // Subsequent dictation updates on other tabs must not stop showing again.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  dictation_service_->StartSessionForTesting(
      *other_tab_, dictation::TargetDetails(),
      dictation::DictationSessionEntryPoint::kContextMenu);
  dictation_service_->session_controller()->EndSessionSynchronously();
}

TEST_F(TabUnderlineControllerTest, MultipleSourcesKeepUnderlineUntilAllHidden) {
  controller_->Initialize(&ui_delegate_);
  controller_->OnUiReady();

  // Show via ContextualTasks and Dictation.
  context_provider_.NotifyObservers({tab_->GetHandle()});
  dictation_service_->StartSessionForTesting(
      *tab_, dictation::TargetDetails(),
      dictation::DictationSessionEntryPoint::kContextMenu);
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Removing ContextualTasks keeps the underline visible because Dictation is
  // still active.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  context_provider_.NotifyObservers({});
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Ending Dictation now hides the underline.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(1);
  dictation_service_->session_controller()->EndSessionSynchronously();
  EXPECT_FALSE(ui_delegate_.IsShowing());
}

TEST_F(TabUnderlineControllerTest,
       GlicPinnedUnderlinePreservedWhenContextualTasksHides) {
  ON_CALL(glic_service_->mock_coordinator(), IsAnyPanelShowing())
      .WillByDefault(Return(true));
  glic_service_->SetContextAccessIndicator(true);
  glic_service_->sharing_manager().SetPinnedTabs({tab_.get()});

  controller_->Initialize(&ui_delegate_);

  // ContextualTasks shows the underline before OnUiReady() syncs Glic state.
  context_provider_.NotifyObservers({tab_->GetHandle()});
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // OnUiReady() sees the underline already showing and the tab pinned while
  // Glic is showing, so it animates and records kGlic as an active source.
  EXPECT_CALL(ui_delegate_, ResetAnimationCycle()).Times(1);
  controller_->OnUiReady();
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Removing ContextualTasks must not hide the underline while Glic keeps it
  // pinned.
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(0);
  context_provider_.NotifyObservers({});
  EXPECT_TRUE(ui_delegate_.IsShowing());

  // Unpinning from Glic now ramps down the underline.
  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(1);
  glic_service_->sharing_manager().SetPinnedTabs({});
  EXPECT_FALSE(ui_delegate_.IsShowing());
}

TEST_F(TabUnderlineControllerTest,
       GlicNoOpPinnedTabsChangedDoesNotHideUnderline) {
  controller_->Initialize(&ui_delegate_);
  controller_->OnUiReady();

  // Pinning/unpinning another tab while this tab is hidden must not call
  // StartRampingDown() or StopShowing().
  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(0);
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  glic_service_->sharing_manager().SetPinnedTabs({other_tab_.get()});
  glic_service_->sharing_manager().SetPinnedTabs({});
  testing::Mock::VerifyAndClearExpectations(&ui_delegate_);

  // When this tab is underlined by ContextualTasks, unpinned Glic updates must
  // not hide it.
  context_provider_.NotifyObservers({tab_->GetHandle()});
  EXPECT_TRUE(ui_delegate_.IsShowing());

  EXPECT_CALL(ui_delegate_, StartRampingDown()).Times(0);
  EXPECT_CALL(ui_delegate_, StopShowing()).Times(0);
  glic_service_->sharing_manager().SetPinnedTabs({other_tab_.get()});
  glic_service_->sharing_manager().SetPinnedTabs({});
  EXPECT_TRUE(ui_delegate_.IsShowing());
}

}  // namespace
}  // namespace glic

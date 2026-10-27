// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/indigo/indigo_agent_host.h"
#include "chrome/browser/indigo/indigo_cue_target.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_metrics.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/indigo/proto/indigo_prompts.pb.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/mock_signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/public/skill.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace indigo {
namespace {

using ::optimization_guide::OptimizationGuideDecision;
using signin_ui_util::MockSigninUiDelegate;
using ::testing::_;

class FakeGlicSidePanelCoordinator : public glic::GlicSidePanelCoordinator {
 public:
  explicit FakeGlicSidePanelCoordinator(tabs::TabInterface* tab)
      : glic::GlicSidePanelCoordinator(tab) {}

  void Show(const ShowOptions& options) override {}
  void Close(const glic::CloseOptions& options) override {}
  bool IsShowing() const override { return is_showing_; }
  State state() override {
    return is_showing_ ? State::kShown : State::kClosed;
  }
  bool SupportsPeek() const override { return false; }
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override {
    return {};
  }
#if !BUILDFLAG(IS_ANDROID)
  void SetContentsView(std::unique_ptr<views::View> contents_view) override {}
#else
  void SetWebContents(content::WebContents* web_contents) override {}
#endif
  int GetPreferredWidth() override { return 300; }
  bool IsGlicSidePanelActive() override { return is_active_; }

  void SetActive(bool active) { is_active_ = active; }
  void SetShowing(bool showing) { is_showing_ = showing; }

 private:
  bool is_active_ = false;
  bool is_showing_ = false;
};
// Matcher to verify that GlicInvokeOptions has a specific prompt.
auto HasGlicPrompt(std::string_view prompt) {
  return ::testing::Field("prompts", &glic::GlicInvokeOptions::prompts,
                          ::testing::ElementsAre(prompt));
}
class FakeDocumentMetadata : public blink::mojom::DocumentMetadata {
 public:
  FakeDocumentMetadata() = default;
  ~FakeDocumentMetadata() override = default;

  void GetEntities(GetEntitiesCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  void ClassifyProductDetails(
      const std::vector<std::string>& allowed_keywords,
      const std::vector<std::string>& blocked_keywords,
      ClassifyProductDetailsCallback callback) override {
    last_allowed_keywords_ = allowed_keywords;
    last_blocked_keywords_ = blocked_keywords;
    std::move(callback).Run(result_ ? result_.Clone() : nullptr);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void SetResult(blink::mojom::ProductClassificationResultPtr result) {
    result_ = std::move(result);
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  const std::vector<std::string>& last_allowed_keywords() const {
    return last_allowed_keywords_;
  }
  const std::vector<std::string>& last_blocked_keywords() const {
    return last_blocked_keywords_;
  }

 private:
  blink::mojom::ProductClassificationResultPtr result_;
  std::vector<std::string> last_allowed_keywords_;
  std::vector<std::string> last_blocked_keywords_;
  base::OnceClosure quit_closure_;
};

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kSignOutSupportedOnPlatform = false;
#else
constexpr bool kSignOutSupportedOnPlatform = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

struct CreateControllerOptions {
  bool expect_register_optimization_types = true;
};

class IndigoPageActionControllerTestBase : public testing::Test {
 protected:
  // Whether the controller delegates its entry points to Contextual Cueing
  // instead of showing them itself (legacy mode).
  virtual bool IsContextualCuesMode() const = 0;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kIndigo,
         {{"indigo_onboarding_url", "https://example.com/onboard"},
          {features::kIndigoAllowForEnterprise.name, "true"}}},
        {features::kIndigoMetadataKeywordHeuristic, {}}};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsContextualCuesMode()) {
      enabled_features.emplace_back(contextual_cueing::kContextualCueingV2,
                                    base::FieldTrialParams());
      enabled_features.emplace_back(features::kIndigoContextualCueingV2,
                                    base::FieldTrialParams());
    } else {
      disabled_features.emplace_back(contextual_cueing::kContextualCueingV2);
      disabled_features.emplace_back(features::kIndigoContextualCueingV2);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
    // Command line changes are automatically reset between unit tests by
    // base::TestSuite's ResetCommandLineBetweenTests listener after all
    // tasks have finished running.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("indigo-script",
                                                              "/dummy/path");
    // SetUpGlobalFeaturesForTesting is required to initialize
    // GlicGlobalEnabling which is checked by GlicEnabling.
    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(true);
  }

  void TearDown() override {
    cue_target_.reset();
    controller_.reset();
    page_action_controller_.reset();
    fake_glic_side_panel_coordinator_.reset();
    tab_interface_.reset();
    mock_optimization_guide_ = nullptr;
    mock_glic_instance_.reset();
    mock_glic_keyed_service_ = nullptr;
    mock_skills_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    profile_.reset();
    testing_profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  void CreateController(CreateControllerOptions options = {}) {
    CHECK(!controller_);

    if (!profile_) {
      TestingProfile::Builder builder;
      builder.AddTestingFactory(
          OptimizationGuideKeyedServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<MockOptimizationGuideKeyedService>>();
          }));
      builder.AddTestingFactory(
          skills::SkillsServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<skills::MockSkillsService>>();
          }));
      builder.AddTestingFactory(
          glic::GlicKeyedServiceFactory::GetInstance(),
          base::BindRepeating(
              [](ProfileManager* profile_manager,
                 glic::GlicProfileManager* glic_profile_manager,
                 content::BrowserContext* context)
                  -> std::unique_ptr<KeyedService> {
                return std::make_unique<
                    testing::NiceMock<glic::MockGlicKeyedService>>(
                    context,
                    IdentityManagerFactory::GetForProfile(
                        Profile::FromBrowserContext(context)),
                    profile_manager, glic_profile_manager, nullptr, nullptr);
              },
              testing_profile_manager_->profile_manager(),
              glic::GlicProfileManager::GetInstance()));
      profile_ = IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(builder);

      identity_test_env_adaptor_ =
          std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
              profile_.get());
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin);

      mock_optimization_guide_ =
          static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
              OptimizationGuideKeyedServiceFactory::GetForProfile(
                  profile_.get()));

      mock_skills_service_ =
          static_cast<testing::NiceMock<skills::MockSkillsService>*>(
              skills::SkillsServiceFactory::GetForProfile(profile_.get()));
      CHECK(mock_skills_service_);

      SetModelExecutionCapability(true);

      mock_glic_keyed_service_ =
          static_cast<testing::NiceMock<glic::MockGlicKeyedService>*>(
              glic::GlicKeyedServiceFactory::GetGlicKeyedService(
                  profile_.get(), /*create=*/true));
      CHECK(mock_glic_keyed_service_);
      mock_glic_instance_ = std::make_unique<glic::MockGlicInstance>();
      ON_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(testing::_, testing::_))
          .WillByDefault(testing::Return(mock_glic_instance_->GetWeakPtr()));
    }

    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
    tabs::TabLookupFromWebContents::CreateForWebContents(
        tab_interface_->GetContents(), tab_interface_.get());
    ON_CALL(*tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&mock_browser_window_interface_));
    ON_CALL(testing::Const(*tab_interface_), GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&mock_browser_window_interface_));
    ON_CALL(*tab_interface_, IsVisible()).WillByDefault(testing::Return(true));
    ON_CALL(testing::Const(*tab_interface_), IsVisible())
        .WillByDefault(testing::Return(true));

    ON_CALL(testing::Const(mock_browser_window_interface_),
            GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(browser_window_user_data_host_));

    page_action_controller_ =
        std::make_unique<page_actions::MockPageActionController>();

    // The service is only created when
    // `optimization_guide::features::IsOptimizationHintsEnabled()` returns
    // true.
    if (mock_optimization_guide_) {
      if (options.expect_register_optimization_types) {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(testing::ElementsAre(
                        optimization_guide::proto::OptimizationType::INDIGO)));
      } else {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(::testing::Contains(
                        optimization_guide::proto::OptimizationType::INDIGO)))
            .Times(0);
      }
    } else {
      EXPECT_FALSE(options.expect_register_optimization_types)
          << "Cannot expect registration when OptimizationGuideKeyedService "
             "was not created";
    }
    fake_glic_side_panel_coordinator_ =
        std::make_unique<FakeGlicSidePanelCoordinator>(tab_interface_.get());
    controller_ = std::make_unique<IndigoPageActionController>(
        *tab_interface_, *page_action_controller_);

    // Mirrors the tab features, which register the Indigo cue target with
    // Contextual Cueing when both flags are enabled.
    if (base::FeatureList::IsEnabled(features::kIndigoContextualCueingV2) &&
        base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2)) {
      IndigoService* indigo_service =
          IndigoServiceFactory::GetForProfile(profile_.get());
      CHECK(indigo_service);
      cue_target_ =
          std::make_unique<IndigoCueTarget>(*indigo_service, *tab_interface_);
    }
  }

  void SetupHeuristicConfig() {
    auto* service = IndigoServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(service);
    IndigoService::ConfigData config;
    config.allowed_origins = {
        url::Origin::Create(GURL("https://allowed1.com")),
        url::Origin::Create(GURL("https://allowed2.com")),
    };
    config.allowed_keywords = {"keyword1", "keyword2"};
    config.blocked_keywords = {"blocked1", "blocked2"};
    service->SetConfigForTesting(std::move(config));
  }

  void ExpectOptimizationGuideDecision(const GURL& url,
                                       OptimizationGuideDecision decision) {
    EXPECT_CALL(
        *mock_optimization_guide_,
        CanApplyOptimization(
            url, optimization_guide::proto::OptimizationType::INDIGO,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            decision, optimization_guide::OptimizationMetadata()));
  }

  void SetupEligibleAndOnboarded() {
    profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);
    IndigoServiceFactory::GetForProfile(profile_.get())
        ->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
            [](IndigoService::RemoteEligibilityCallback callback) {
              std::move(callback).Run(
                  RemoteEligibility{.is_service_supported_for_account = true,
                                    .has_user_image = true});
            }));
  }

  void SetModelExecutionCapability(bool can_use_model_execution_features) {
    signin::IdentityManager* identity_manager =
        identity_test_env_adaptor_->identity_test_env()->identity_manager();
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(
        can_use_model_execution_features);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void SetupComponentWithPrompts(
      const base::FilePath& temp_dir_path,
      const std::vector<std::pair<std::string, std::string>>& prompts) {
    chrome::aix::indigo::IndigoPrompts proto;
    for (const auto& [key, prompt_text] : prompts) {
      auto* prompt = proto.add_prompts();
      prompt->set_key(key);
      prompt->set_prompt(prompt_text);
    }

    base::FilePath prompts_path =
        temp_dir_path.Append(FILE_PATH_LITERAL("indigo_prompts.bin"));
    std::string serialized;
    ASSERT_TRUE(proto.SerializeToString(&serialized));
    ASSERT_TRUE(base::WriteFile(prompts_path, serialized));

    base::test::TestFuture<void> prompts_loaded_future;
    IndigoServiceFactory::GetForProfile(profile_.get())
        ->SetPromptsLoadedCallbackForTesting(
            prompts_loaded_future.GetCallback());

    component_updater::IndigoComponentInstallerPolicy policy;
    policy.ComponentReady(base::Version("1.0"), temp_dir_path,
                          base::DictValue());

    EXPECT_TRUE(prompts_loaded_future.Wait());
  }

  // Expects the controller not to show or hide any entry point through the
  // page action controller.
  void ExpectNoEntryPointUi() {
    EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
    EXPECT_CALL(*page_action_controller_, Hide(_)).Times(0);
    EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);
    EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(_, _)).Times(0);
  }

  // Expects the Indigo entry point to be shown for the current page. In legacy
  // mode, the controller shows its page action itself. In contextual cues mode,
  // it doesn't show anything, as showing the cue is up to Contextual Cueing.
  void ExpectEntryPointShown() {
    if (IsContextualCuesMode()) {
      ExpectNoEntryPointUi();
    } else {
      EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
    }
  }

  // Expects the Indigo entry point to be hidden for the current page. In legacy
  // mode, the controller hides its page action itself. In contextual cues mode,
  // it doesn't hide anything, as hiding the cue is up to Contextual Cueing.
  void ExpectEntryPointHidden() {
    if (IsContextualCuesMode()) {
      ExpectNoEntryPointUi();
    } else {
      EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));
    }
  }

  // Returns whether the controller considers the current page eligible for the
  // Indigo cue, as reported to Contextual Cueing. Waits for a pending decision
  // to be made or to time out.
  bool GetEligibilityForCueing() {
    base::test::TestFuture<bool> future;
    controller_->CheckEligibilityForCueing(future.GetCallback());
    return future.Get();
  }

  // In contextual cues mode, simulates Contextual Cueing showing the Indigo cue
  // for the current page as a proactive anchored message, through the Indigo
  // cue target. Does nothing in legacy mode, where the controller shows the
  // entry point itself.
  void ShowCueIfDelegatedToContextualCueing() {
    if (!IsContextualCuesMode()) {
      return;
    }
    CHECK(cue_target_);
    base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
        eligibility_future;
    cue_target_->CheckEligibility(tab_interface_->GetContents()->GetWeakPtr(),
                                  contextual_cueing::CueIntrusiveness::kLoud,
                                  eligibility_future.GetCallback());
    auto [eligible, content_generator] = eligibility_future.Take();
    ASSERT_TRUE(eligible);
    ASSERT_TRUE(content_generator);

    base::test::TestFuture<
        std::optional<optimization_guide::proto::ContextualCue>>
        cue_future;
    std::move(content_generator).Run(cue_future.GetCallback());
    ASSERT_TRUE(cue_future.Get().has_value());

    cue_target_->OnAnchoredMessageShown(
        page_actions::PageActionPriorityCategory::kContextualCue);
  }

  // Navigates to `url`, which the optimization guide allows Indigo on, and
  // shows the anchored message for it (proactively in contextual cues mode, or
  // reactively by clicking the suggestion chip in legacy mode).
  void NavigateAndShowAnchoredMessage(const GURL& url) {
    ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
    if (!IsContextualCuesMode()) {
      EXPECT_CALL(*page_action_controller_,
                  ShowSuggestionChip(kActionIndigo, _));
    }
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        tab_interface_->GetContents(), url);
    if (IsContextualCuesMode()) {
      ShowCueIfDelegatedToContextualCueing();
    } else {
      EXPECT_CALL(*page_action_controller_,
                  ShowAnchoredMessage(kActionIndigo, _));
      controller_->InvokeAction(EntryPoint::kSuggestionChip);
    }
  }

  // Simulates the user clicking the anchored message, which is forwarded to
  // the controller by its page action in legacy mode, and by the Indigo cue
  // target in contextual cues mode.
  void ClickAnchoredMessage() {
    if (IsContextualCuesMode()) {
      CHECK(cue_target_);
      cue_target_->OnAnchoredMessageClicked(std::monostate());
    } else {
      controller_->InvokeAction(EntryPoint::kAnchoredMessage);
    }
  }

  // Must be declared before `task_environment_` so background tasks are
  // stopped before scoped state is torn down.
  base::test::ScopedFeatureList feature_list_;
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting
      scoped_glic_bypass_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ChromeLayoutProvider layout_provider_;
#if BUILDFLAG(IS_CHROMEOS)
  // Needed because TestWebContents ends up creating BTM classes which depend
  // on this on ChromeOS.
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<testing::NiceMock<glic::MockGlicKeyedService>>
      mock_glic_keyed_service_ = nullptr;
  std::unique_ptr<glic::MockGlicInstance> mock_glic_instance_;
  raw_ptr<testing::NiceMock<skills::MockSkillsService>> mock_skills_service_ =
      nullptr;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_;
  ui::UnownedUserDataHost browser_window_user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<page_actions::FakeTabInterface> tab_interface_;
  std::unique_ptr<page_actions::MockPageActionController>
      page_action_controller_;
  std::unique_ptr<IndigoPageActionController> controller_;
  std::unique_ptr<FakeGlicSidePanelCoordinator>
      fake_glic_side_panel_coordinator_;
  // Only created when the entry points are delegated to Contextual Cueing.
  std::unique_ptr<IndigoCueTarget> cue_target_;
};

// Runs each test both in legacy mode and in contextual cues mode. In contextual
// cues mode, Contextual Cueing shows and hides the entry point, so tests check
// the page's eligibility instead (see `GetEligibilityForCueing()`).
// TODO(b/573154132): Once legacy mode is removed, rename tests whose
// names say the entry point shows or hides (e.g.
// `ShowsWhenOptimizationGuideReturnsTrue`), since they only check eligibility.
class IndigoPageActionControllerTest
    : public IndigoPageActionControllerTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  bool IsContextualCuesMode() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         IndigoPageActionControllerTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "ContextualCues" : "Legacy";
                         });

// Tests for the entry points that the controller shows itself, which only
// happens in legacy mode.
class IndigoPageActionControllerLegacyTest
    : public IndigoPageActionControllerTestBase {
 protected:
  bool IsContextualCuesMode() const override { return false; }
};

TEST_P(IndigoPageActionControllerTest, ShowsWhenOptimizationGuideReturnsTrue) {
  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, HidesWhenOptimizationGuideReturnsFalse) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());

  // Now expect Hide when navigating to a page where the decision is false.
  ExpectEntryPointHidden();

  GURL url2("https://example2.com");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Commit();
  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, UpdatesOnSameDocumentNavigation) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com/page1");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());

  // Now expect Hide when performing a same-document navigation to a page where
  // the decision is false.
  ExpectEntryPointHidden();

  GURL url2("https://example.com/page2");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();
  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, IgnoresFragmentOnlyNavigation) {
  CreateController();

  GURL url1("https://example.com/page");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());

  testing::Mock::VerifyAndClearExpectations(mock_optimization_guide_);
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Now perform a same-document navigation that only changes the fragment.
  // We expect NO calls to mock_optimization_guide_ or page_action_controller_.
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          _, _,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  EXPECT_CALL(*page_action_controller_, Hide(_)).Times(0);

  GURL url2("https://example.com/page#fragment");
  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();

  // The page is still eligible.
  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, QueriesOptimizationGuideOnReload) {
  CreateController();

  GURL url("https://example.com");

  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());

  testing::Mock::VerifyAndClearExpectations(mock_optimization_guide_);
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Reloading the same URL should query optimization guide again.
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  ExpectEntryPointShown();

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation2->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest,
       HidesWhenOptimizationHintsFeatureIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHints);

  CreateController({.expect_register_optimization_types = false});

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  GURL url("https://example.com");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Without an optimization guide decision, the eligibility check times out.
  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, HidesWhenNotSignedIn) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  CreateController();
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, HidesWhenCapabilityIsFalse) {
  CreateController();
  SetModelExecutionCapability(false);

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, ShowsWhenCapabilityIsTrue) {
  CreateController();
  SetModelExecutionCapability(true);

  ExpectEntryPointShown();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, UpdatesWhenCapabilityChanges) {
  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  SetModelExecutionCapability(true);

  // Show the action initially.
  ExpectEntryPointShown();
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Hide when capability becomes false.
  ExpectEntryPointHidden();
  SetModelExecutionCapability(false);
  EXPECT_FALSE(GetEligibilityForCueing());
  ::testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Show again when capability becomes true.
  ExpectEntryPointShown();
  SetModelExecutionCapability(true);
  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, UpdatesWhenAccountChanges) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  SetModelExecutionCapability(true);

  // Show the action initially.
  ExpectEntryPointShown();
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Hide when signing out.
  ExpectEntryPointHidden();
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(GetEligibilityForCueing());
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Show again when signing back in.
  ExpectEntryPointShown();
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "user2@example.com", signin::ConsentLevel::kSignin);
  SetModelExecutionCapability(true);
  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, InvokeActionTriggersEligibilityCheck) {
  CreateController();

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  base::HistogramTester histogram_tester;
  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
  EXPECT_TRUE(fetcher_called.Wait());
  histogram_tester.ExpectTotalCount("Indigo.Discovery.EligibilityCheck.Latency",
                                    1);
}

TEST_P(IndigoPageActionControllerTest, OnboardingSuccessTriggersContinuation) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

  base::UserActionTester user_action_tester;

  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  // Initial state: eligible but needs onboarding. This should show the dialog.
  CombinedEligibility eligibility;
  eligibility.local_eligibility = LocalEligibility::kEligible;
  eligibility.remote_eligibility = RemoteEligibility{
      .is_service_supported_for_account = true, .has_user_image = false};
  eligibility.has_onboarded_pref = false;

  base::OnceCallback<void(const OnboardingResult&)> captured_callback;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_callback = std::move(callback);
            return nullptr;
          }));

  IndigoPageActionController::TestApi(controller_.get())
      .CheckEligibilityForOnboarding(
          base::TimeTicks::Now(),
          IndigoTransformationTriggerSource::kPageAction, eligibility);

  ASSERT_TRUE(!captured_callback.is_null());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Trigger"), 1);

  // Now simulate the dialog closing with success.
  std::move(captured_callback).Run(result);

  // Closing with success set the pref and trigger a re-fetch for continuation.
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_TRUE(fetcher_called.Wait());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 1);
}

TEST_P(IndigoPageActionControllerTest,
       OnboardingSuccessWithToastRetryDoesNotOpenGlic) {
  CreateController();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  // Set up an active page so IndigoAgentHost can be invoked upon continuation.
  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Glic should NOT be invoked for non-page-action triggers.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{
                .is_service_supported_for_account = true,
                .has_user_image = true,
            });
          }));

  // Initial state: eligible but needs onboarding. This should show the dialog.
  CombinedEligibility eligibility;
  eligibility.local_eligibility = LocalEligibility::kEligible;
  eligibility.remote_eligibility = RemoteEligibility{
      .is_service_supported_for_account = true, .has_user_image = false};
  eligibility.has_onboarded_pref = false;

  base::OnceCallback<void(const OnboardingResult&)> captured_callback;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_callback = std::move(callback);
            return nullptr;
          }));

  IndigoPageActionController::TestApi(controller_.get())
      .CheckEligibilityForOnboarding(
          base::TimeTicks::Now(),
          IndigoTransformationTriggerSource::kErrorToastRetry, eligibility);

  ASSERT_TRUE(!captured_callback.is_null());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Trigger"), 1);

  // Now simulate the dialog closing with success.
  std::move(captured_callback).Run(result);

  // Closing with success should set the pref and trigger a re-fetch for
  // continuation.
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_TRUE(fetcher_called.Wait());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 1);

  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.TriggerSource",
      IndigoTransformationTriggerSource::kErrorToastRetry, 1);
}

TEST_P(IndigoPageActionControllerTest, OnboardingCancelledDoesNotTrigger) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

  base::UserActionTester user_action_tester;

  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = false;

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  IndigoPageActionController::TestApi(controller_.get())
      .CheckOnboardingResult(OnboardingDisposition::kDefault,
                             IndigoTransformationTriggerSource::kPageAction,
                             result);

  EXPECT_FALSE(fetcher_called.IsReady());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 0);
}

TEST_P(IndigoPageActionControllerTest,
       OnReplaceOriginalPhotoTriggersOnboardingWithParam) {
  CreateController();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);

  base::UserActionTester user_action_tester;

  GURL captured_url;
  base::OnceCallback<void(const OnboardingResult&)> captured_callback;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_url = url;
            captured_callback = std::move(callback);
            return nullptr;
          }));

  controller_->OnReplaceOriginalPhoto(nullptr);

  EXPECT_EQ(captured_url,
            GURL("https://example.com/onboard?toyut=chrome-mi&toyri=1"));
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.ReplaceImage.Trigger"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Trigger"), 0);

  // Simulate success.
  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;
  std::move(captured_callback).Run(result);

  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.ReplaceImage.Complete"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 0);
}

TEST_P(IndigoPageActionControllerTest,
       OnboardingAppendsToyutChromeMiWhenModelImprovementAllowed) {
  CreateController();
  profile_->GetPrefs()->SetInteger(prefs::kIndigoPolicy,
                                   prefs::Policy::kAllowed);

  GURL captured_url;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_url = url;
            return nullptr;
          }));

  controller_->OnReplaceOriginalPhoto(nullptr);
  EXPECT_EQ(captured_url,
            GURL("https://example.com/onboard?toyut=chrome-mi&toyri=1"));
}

TEST_P(IndigoPageActionControllerTest,
       OnboardingAppendsToyutChromeNomiWhenModelImprovementDisallowed) {
  CreateController();
  profile_->GetPrefs()->SetInteger(
      prefs::kIndigoPolicy, prefs::Policy::kAllowedWithoutModelImprovement);

  GURL captured_url;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_url = url;
            return nullptr;
          }));

  controller_->OnReplaceOriginalPhoto(nullptr);
  EXPECT_EQ(captured_url,
            GURL("https://example.com/onboard?toyut=chrome-nomi&toyri=1"));
}

TEST_P(IndigoPageActionControllerTest, OnCloseResetsReplacements) {
  CreateController();

  GURL url("https://example.com");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  content::WebContents* web_contents = tab_interface_->GetContents();
  auto* manager = IndigoImageReplacementManager::GetOrCreateForPage(
      web_contents->GetPrimaryPage());

  base::test::TestFuture<void> disconnect_future;

  class FakeImageReplacement : public blink::mojom::ImageReplacement {
   public:
    FakeImageReplacement() = default;
    void StartReplacement(
        mojo::PendingRemote<blink::mojom::ImageReplacementHost> host,
        std::optional<int32_t> tracked_element_feature_id) override {
      // Do nothing.
    }
    void RenderReplacement() override {}
  };

  FakeImageReplacement fake_replacement;
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&fake_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  receiver.set_disconnect_handler(disconnect_future.GetCallback());

  controller_->OnClose(nullptr);

  EXPECT_TRUE(disconnect_future.Wait());
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionOpensGlicForAnchoredMessage) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce(::testing::Return(mock_glic_instance_->GetWeakPtr()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  histogram_tester.ExpectUniqueSample(kMaybeInvokeGlicResultHistogramName,
                                      IndigoMaybeInvokeGlicResult::kInvoked, 1);
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionSkipsGlicIfOngoingConversation) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  glic::MockGlicInstance mock_glic_instance;
  EXPECT_CALL(*mock_glic_keyed_service_,
              GetInstanceForTab(tab_interface_.get()))
      .WillOnce(::testing::Return(&mock_glic_instance));
  EXPECT_CALL(mock_glic_instance, conversation_id())
      .WillOnce(::testing::Return(std::optional<std::string>("convo123")));

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  histogram_tester.ExpectUniqueSample(
      kMaybeInvokeGlicResultHistogramName,
      IndigoMaybeInvokeGlicResult::kExistingConversation, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.TriggerSource",
      IndigoTransformationTriggerSource::kPageAction, 1);
}

TEST_P(IndigoPageActionControllerTest, MaybeInvokeGlicRecordsFeatureDisabled) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(features::kIndigoOpenGlic);

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  histogram_tester.ExpectUniqueSample(
      kMaybeInvokeGlicResultHistogramName,
      IndigoMaybeInvokeGlicResult::kFeatureDisabled, 1);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       MaybeInvokeGlicRecordsAlreadyShowing) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  fake_glic_side_panel_coordinator_->SetShowing(true);
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::HistogramTester histogram_tester;
  controller_->InvokeAction(EntryPoint::kSuggestionChip);

  histogram_tester.ExpectUniqueSample(
      kMaybeInvokeGlicResultHistogramName,
      IndigoMaybeInvokeGlicResult::kAlreadyShowing, 1);
}

TEST_P(IndigoPageActionControllerTest, MaybeInvokeGlicRecordsPromptEmpty) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(features::kIndigoOpenGlic);

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  histogram_tester.ExpectUniqueSample(kMaybeInvokeGlicResultHistogramName,
                                      IndigoMaybeInvokeGlicResult::kPromptEmpty,
                                      1);
}

TEST_P(IndigoPageActionControllerTest, MaybeInvokeGlicRecordsInvokeRejected) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  histogram_tester.ExpectUniqueSample(
      kMaybeInvokeGlicResultHistogramName,
      IndigoMaybeInvokeGlicResult::kInvokeRejected, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            1);
}

TEST_P(IndigoPageActionControllerTest,
       PanelActuallyShowingOnCallbackRecordsTrueWhenShowing) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  glic::GlicInvokeOptions captured_options(
      glic::mojom::InvocationSource::kIndigoPageAction);

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce([&](glic::InvokeWithAutoSubmitPasskey passkey,
                    glic::GlicInvokeOptions options) {
        captured_options = std::move(options);
        return mock_glic_instance_->GetWeakPtr();
      });

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  ASSERT_FALSE(captured_options.on_panel_opened.is_null());
  fake_glic_side_panel_coordinator_->SetShowing(true);
  std::move(captured_options.on_panel_opened).Run();

  histogram_tester.ExpectUniqueSample(
      kPanelActuallyShowingOnCallbackHistogramName, true, 1);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       SuggestionChipClickShowsAnchoredMessageAndThenOpensGlic) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  // Glic should only be invoked once, on the second click.
  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce(::testing::Return(mock_glic_instance_->GetWeakPtr()));

  GURL url("https://example.com/1");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // The first click on the suggestion chip should show the anchored message.
  EXPECT_CALL(*page_action_controller_,
              SetAnchoredMessageText(kActionIndigo, _));
  EXPECT_CALL(*page_action_controller_,
              SetAnchoredMessageIcon(kActionIndigo, _));
  EXPECT_CALL(
      *page_action_controller_,
      SetAnchoredMessageAction(
          kActionIndigo, page_actions::AnchoredMessageActionIconType::kClose,
          testing::_));
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(kActionIndigo, _));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);

  // The second click should now trigger Glic.
  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_P(IndigoPageActionControllerTest, InvokeActionTriggerReauthWhenPaused) {
  CreateController();

  // Set the account in paused state.
  identity_test_env_adaptor_->identity_test_env()
      ->SetInvalidRefreshTokenForPrimaryAccount();

  base::HistogramTester histogram_tester;

  // We expect ShowReauthUI to be called on the mock delegate.
  testing::StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);

  EXPECT_CALL(
      mock_signin_ui_delegate,
      ShowReauthUI(profile_.get(), "user@example.com",
                   /*enable_history_sync=*/false,
                   signin_metrics::AccessPoint::kIndigo,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO));

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);

  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.Result",
      static_cast<int>(
          IndigoTransformationResult::kRefreshTokenInPersistentErrorState),
      1);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_P(IndigoPageActionControllerTest, InvokeActionOpensGlicWithProtoPrompt) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();

  SetupComponentWithPrompts(temp_dir.GetPath(), {{"v5", "proto test prompt"}});

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt_key", "v5"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("proto test prompt")))
      .WillOnce(::testing::Return(mock_glic_instance_->GetWeakPtr()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  ClickAnchoredMessage();
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionOpensGlicWithOverridePromptPrecedence) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();

  SetupComponentWithPrompts(temp_dir.GetPath(), {{"v5", "proto test prompt"}});

  base::test::ScopedFeatureList local_feature_list;
  // Both override and key are set. Override should win.
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "override prompt"},
                                  {"indigo_glic_prompt_key", "v5"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("override prompt")))
      .WillOnce(::testing::Return(mock_glic_instance_->GetWeakPtr()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  ClickAnchoredMessage();
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionDoesNotOpenGlicIfPromptNotFound) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();

  SetupComponentWithPrompts(temp_dir.GetPath(), {});

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt_key", "v5"}});

  // Expect NO call to InvokeWithAutoSubmit.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  ClickAnchoredMessage();
}

TEST_P(IndigoPageActionControllerTest, InvokeActionOpensGlicWithSkill) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_skill_id", "test_skill_id"}});

  // Mock the skill lookup
  skills::Skill mock_skill;
  mock_skill.id = "test_skill_id";
  mock_skill.prompt = "skill test prompt";
  mock_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;

  EXPECT_CALL(*mock_skills_service_, GetSkillById("test_skill_id"))
      .WillOnce(testing::Return(&mock_skill));

  // Verify Glic is called with the skill prompt
  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("skill test prompt")))
      .WillOnce(::testing::Return(mock_glic_instance_->GetWeakPtr()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  ClickAnchoredMessage();
}

TEST_F(IndigoPageActionControllerLegacyTest,
       InvokeActionSuggestionChipRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  controller_->InvokeAction(EntryPoint::kSuggestionChip);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionChipClickAction), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kAnchoredMessageClickAction), 0);
  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram, IndigoPageActionEntryPoint::kSuggestionChip,
      1);
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionAnchoredMessageRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  IndigoPageActionController::TestApi(controller_.get())
      .SetLastAnchoredMessagePriority(
          page_actions::PageActionPriorityCategory::kContextualCue);
  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionChipClickAction), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(kAnchoredMessageClickAction), 1);
  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram,
      IndigoPageActionEntryPoint::kProactiveAnchoredMessage, 1);
}

TEST_P(IndigoPageActionControllerTest, InvokeActionErrorToastRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  controller_->InvokeAction(EntryPoint::kErrorToast);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionChipClickAction), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(kAnchoredMessageClickAction), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(kErrorToastRetryClickAction), 1);
  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram, IndigoPageActionEntryPoint::kErrorToast, 1);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       OnPageActionAnchoredMessageShownReactive) {
  CreateController();

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  IndigoPageActionController::TestApi(controller_.get())
      .SetLastAnchoredMessagePriority(
          page_actions::PageActionPriorityCategory::kUserInteraction);

  page_actions::PageActionState state;
  state.action_id = kActionIndigo;
  state.anchored_message_showing = true;
  controller_->OnPageActionAnchoredMessageShown(state);

  EXPECT_EQ(
      user_action_tester.GetActionCount(kReactiveAnchoredMessageShowAction), 1);
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram,
      IndigoPageActionEntryPoint::kReactiveAnchoredMessage, 1);
}

TEST_F(IndigoPageActionControllerLegacyTest, OnPageActionChipShown) {
  CreateController();

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  page_actions::PageActionState state;
  state.action_id = kActionIndigo;
  state.chip_showing = true;
  controller_->OnPageActionChipShown(state);

  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionChipShowAction), 1);
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kSuggestionChip,
      1);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       InvokeActionShowsAnchoredMessageWhenSidePanelClosed) {
  CreateController();
  SetupEligibleAndOnboarded();

  // Simulate that the side panel is closed.
  fake_glic_side_panel_coordinator_->SetShowing(false);

  // The click should show the anchored message.
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(kActionIndigo, _));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       InvokeActionSuggestionChipSecondClickDoesNotOpenGlic) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  // Simulate that the side panel is closed.
  fake_glic_side_panel_coordinator_->SetShowing(false);

  // 1st click: show the anchored message.
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(kActionIndigo, _));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);

  // The 2nd click should NOT open Glic, but should show the anchored message
  // again.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(kActionIndigo, _));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);
}

TEST_F(IndigoPageActionControllerLegacyTest,
       InvokeActionDoesNotOpenGlicWhenSidePanelOpen) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  // Simulate that the Glic side panel is open.
  fake_glic_side_panel_coordinator_->SetShowing(true);

  // Glic should NOT be invoked.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  // We also should NOT show the anchored message, as we proceed to Invoke
  // directly.
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);

  // Need to set an active page so IndigoAgentHost can be created and Invoked.
  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kSuggestionChip);
}

TEST_P(IndigoPageActionControllerTest, InvokeActionErrorToastDoesNotOpenGlic) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  // Need to set an active page so IndigoAgentHost can be created and Invoked.
  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Glic should NOT be invoked.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  // We also should NOT show the anchored message.
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);

  controller_->InvokeAction(EntryPoint::kErrorToast);
}

TEST_P(IndigoPageActionControllerTest, DelayAgentInvokeUntilGlicPanelOpened) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  glic::GlicInvokeOptions captured_options(
      glic::mojom::InvocationSource::kIndigoPageAction);

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce([&](glic::InvokeWithAutoSubmitPasskey passkey,
                    glic::GlicInvokeOptions options) {
        captured_options = std::move(options);
        return mock_glic_instance_->GetWeakPtr();
      });

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  // The agent should not be triggered yet because we haven't executed
  // on_panel_opened.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            0);

  // Now execute the panel opened callback.
  ASSERT_FALSE(captured_options.on_panel_opened.is_null());
  std::move(captured_options.on_panel_opened).Run();

  histogram_tester.ExpectUniqueSample(
      kPanelActuallyShowingOnCallbackHistogramName, false, 1);

  // The agent should still not be triggered because of the 500ms delay.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            0);

  // Fast-forward time by 300ms.
  task_environment_.FastForwardBy(base::Milliseconds(300));

  // The agent should now be triggered.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.TriggerSource",
      IndigoTransformationTriggerSource::kPageAction, 1);
}

TEST_P(IndigoPageActionControllerTest,
       DelayAgentInvokeUntilGlicPanelOpenedWithCustomDelay) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeaturesAndParameters(
      {{features::kIndigoOpenGlic,
        {{"indigo_glic_prompt", "test prompt"},
         {"indigo_glic_trigger_delay", "500ms"}}}},
      {});

  glic::GlicInvokeOptions captured_options(
      glic::mojom::InvocationSource::kIndigoPageAction);

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce([&](glic::InvokeWithAutoSubmitPasskey passkey,
                    glic::GlicInvokeOptions options) {
        captured_options = std::move(options);
        return mock_glic_instance_->GetWeakPtr();
      });

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  ClickAnchoredMessage();

  // The agent should not be triggered yet because we haven't executed
  // on_panel_opened.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            0);

  // Now execute the panel opened callback.
  ASSERT_FALSE(captured_options.on_panel_opened.is_null());
  std::move(captured_options.on_panel_opened).Run();

  // The agent should still not be triggered.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            0);

  // Fast-forward time by 300ms. It should still NOT be triggered because delay
  // is 500ms.
  task_environment_.FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            0);

  // Fast-forward time by another 200ms (total 500ms).
  task_environment_.FastForwardBy(base::Milliseconds(200));

  // The agent should now be triggered.
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Transformation.Trigger"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.TriggerSource",
      IndigoTransformationTriggerSource::kPageAction, 1);
}

TEST_P(IndigoPageActionControllerTest, HeuristicShowsActionOnSuccess) {
  CreateController();
  SetupHeuristicConfig();

  // URL in allowlist.
  GURL url("https://allowed1.com/product1");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kUnknown);

  FakeDocumentMetadata fake_metadata;
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata.SetResult(std::move(result));

  auto* rfh = tab_interface_->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces = rfh->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces);

  mojo::Receiver<blink::mojom::DocumentMetadata> receiver(&fake_metadata);
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
             mojo::ScopedMessagePipeHandle pipe) {
            receiver->Bind(
                mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                    std::move(pipe)));
          },
          base::Unretained(&receiver)));

  // We expect Show to be called when classification succeeds.
  ExpectEntryPointShown();

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Waits for the classification to complete.
  EXPECT_TRUE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, HeuristicDisabledByFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kIndigoMetadataKeywordHeuristic);

  CreateController();
  SetupHeuristicConfig();

  // URL in allowlist.
  GURL url("https://allowed1.com/product1");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kUnknown);

  auto* rfh = tab_interface_->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces = rfh->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces);

  bool binder_called = false;
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](bool* binder_called, mojo::ScopedMessagePipeHandle pipe) {
            *binder_called = true;
          },
          base::Unretained(&binder_called)));

  // We expect Show to NOT be called because the feature is disabled.
  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_FALSE(GetEligibilityForCueing());
  EXPECT_FALSE(binder_called);
}

TEST_P(IndigoPageActionControllerTest, HeuristicHidesActionOnFailure) {
  CreateController();
  SetupHeuristicConfig();

  // 1. Show it first using OptGuide.
  GURL url1("https://example.com");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // 2. Navigate to allowed URL but heuristic fails. Should Hide.
  GURL url2("https://allowed1.com/product1");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kUnknown);

  FakeDocumentMetadata fake_metadata;
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = false;
  result->blocked_keyword_found = true;
  fake_metadata.SetResult(std::move(result));

  base::RunLoop heuristic_run_loop;
  fake_metadata.SetQuitClosure(heuristic_run_loop.QuitClosure());

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Start();
  navigation2->ReadyToCommit();

  auto* rfh2 = navigation2->GetFinalRenderFrameHost();
  ASSERT_TRUE(rfh2);
  content::RenderFrameHostTester::For(rfh2)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces2 = rfh2->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces2);

  mojo::Receiver<blink::mojom::DocumentMetadata> receiver(&fake_metadata);
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces2);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
             mojo::ScopedMessagePipeHandle pipe) {
            receiver->Bind(
                mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                    std::move(pipe)));
          },
          base::Unretained(&receiver)));

  // Navigation should synchronously trigger Hide.
  ExpectEntryPointHidden();
  // After navigation, Show should NOT be called even after heuristic runs.
  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  navigation2->Commit();

  // Wait for the heuristic to finish.
  heuristic_run_loop.Run();
  EXPECT_FALSE(GetEligibilityForCueing());
}

TEST_P(IndigoPageActionControllerTest, HeuristicIgnoredIfUrlNotAllowed) {
  CreateController();
  SetupHeuristicConfig();

  // 1. Show it first.
  GURL url1("https://example.com");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(GetEligibilityForCueing());
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // 2. Navigate to NOT allowed URL. Should Hide.
  GURL url2("https://disallowed.com/product1");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kUnknown);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Start();
  navigation2->ReadyToCommit();

  auto* rfh2 = navigation2->GetFinalRenderFrameHost();
  ASSERT_TRUE(rfh2);
  content::RenderFrameHostTester::For(rfh2)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces2 = rfh2->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces2);

  bool binder_called = false;
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces2);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](bool* binder_called, mojo::ScopedMessagePipeHandle pipe) {
            *binder_called = true;
          },
          base::Unretained(&binder_called)));

  ExpectEntryPointHidden();

  navigation2->Commit();

  EXPECT_FALSE(GetEligibilityForCueing());
  EXPECT_FALSE(binder_called);
}

TEST_P(IndigoPageActionControllerTest,
       SuggestionChipShownWhenContextualCueingV2Disabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      contextual_cueing::kContextualCueingV2);

  CreateController();
  SetupEligibleAndOnboarded();

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerLegacyTest,
       EntryPointNotShownWhenIndigoContextMenuEnabledWithContextualCueingV2) {
  // This fixture disables `kIndigoContextualCueingV2` while leaving
  // `kIndigoContextMenu` enabled by default. When `kContextualCueingV2` is
  // enabled, delegating chip and anchored message cueing to Contextual Cues
  // still leaves a functioning entry point (the context menu), even though CCV2
  // itself will not cue Indigo. Accordingly, the controller should not show its
  // own page action, anchored message, or suggestion chip, while still
  // reporting the page as eligible for the context menu.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      contextual_cueing::kContextualCueingV2);

  CreateController();
  SetupEligibleAndOnboarded();

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo)).Times(0);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _))
      .Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  EXPECT_TRUE(controller_->IsPageEligible());
}

TEST_P(IndigoPageActionControllerTest, TriggerSource_OptimizationGuide) {
  CreateController();
  SetupEligibleAndOnboarded();
  base::HistogramTester histogram_tester;

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  ExpectEntryPointShown();

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // In contextual cues mode, generating the cue records the trigger source.
  ShowCueIfDelegatedToContextualCueing();

  histogram_tester.ExpectUniqueSample("Indigo.PageAction.TriggerSource",
                                      IndigoTriggerSource::kOptimizationGuide,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.OptimizationGuideDecision",
      OptimizationGuideDecision::kTrue, 1);
}

TEST_P(IndigoPageActionControllerTest, TriggerSource_Heuristic) {
  CreateController();
  SetupEligibleAndOnboarded();
  SetupHeuristicConfig();
  base::HistogramTester histogram_tester;

  // URL in allowlist.
  GURL url("https://allowed1.com/product1");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kUnknown);

  FakeDocumentMetadata fake_metadata;
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata.SetResult(std::move(result));

  auto* rfh = tab_interface_->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces = rfh->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces);

  mojo::Receiver<blink::mojom::DocumentMetadata> receiver(&fake_metadata);
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
             mojo::ScopedMessagePipeHandle pipe) {
            receiver->Bind(
                mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                    std::move(pipe)));
          },
          base::Unretained(&receiver)));

  ExpectEntryPointShown();

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Waits for the classification to complete.
  EXPECT_TRUE(GetEligibilityForCueing());
  // In contextual cues mode, generating the cue records the trigger source.
  ShowCueIfDelegatedToContextualCueing();

  histogram_tester.ExpectUniqueSample(
      "Indigo.PageAction.TriggerSource",
      IndigoTriggerSource::kLocalProductKeywordHeuristic, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", true, 1);
}

TEST_P(IndigoPageActionControllerTest, TriggerSource_Both_PriorityToOptGuide) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  CreateController();
  SetupHeuristicConfig();
  base::HistogramTester histogram_tester;

  // Start signed out so we are not locally eligible.
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();

  // URL in allowlist.
  GURL url("https://allowed1.com/product1");

  optimization_guide::OptimizationGuideDecisionCallback opt_guide_callback;
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::INDIGO,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(
          [&opt_guide_callback](
              const GURL& url,
              optimization_guide::proto::OptimizationType optimization_type,
              optimization_guide::OptimizationGuideDecisionCallback callback) {
            opt_guide_callback = std::move(callback);
          });

  // Heuristic also succeeds.
  FakeDocumentMetadata fake_metadata;
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata.SetResult(std::move(result));

  auto* rfh = tab_interface_->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces = rfh->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces);

  mojo::Receiver<blink::mojom::DocumentMetadata> receiver(&fake_metadata);
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
             mojo::ScopedMessagePipeHandle pipe) {
            receiver->Bind(
                mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                    std::move(pipe)));
          },
          base::Unretained(&receiver)));

  base::RunLoop run_loop;
  fake_metadata.SetQuitClosure(run_loop.QuitClosure());

  // Navigate. This will trigger both but not show because signed out.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Wait for heuristic to complete.
  run_loop.Run();

  // Now we make OptGuide return True.
  std::move(opt_guide_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  // Now we sign in. This should trigger Show.
  ExpectEntryPointShown();

  // Make sure we are eligible.
  SetupEligibleAndOnboarded();

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);

  // In contextual cues mode, generating the cue records the trigger source.
  ShowCueIfDelegatedToContextualCueing();

  histogram_tester.ExpectUniqueSample("Indigo.PageAction.TriggerSource",
                                      IndigoTriggerSource::kOptimizationGuide,
                                      1);
}

TEST_P(IndigoPageActionControllerTest, CheckEligibilityForCueingForced) {
  // Command line changes are automatically reset between unit tests by
  // base::TestSuite's ResetCommandLineBetweenTests listener after all
  // tasks have finished running.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kForceIndigoSwitch);

  CreateController();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

#if !BUILDFLAG(IS_CHROMEOS)
// ChromeOS profiles in browser tests have multi profiles.
TEST_P(IndigoPageActionControllerTest,
       CheckEligibilityForCueingNotLocallyEligible) {
  CreateController();
  // Ensure not locally eligible.
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif

TEST_P(IndigoPageActionControllerTest, CheckEligibilityForCueingOptGuideTrue) {
  CreateController();
  SetupEligibleAndOnboarded();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  // Navigate to set the state.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_P(IndigoPageActionControllerTest,
       CheckEligibilityForCueingOptGuideFalseNoHeuristic) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kIndigoMetadataKeywordHeuristic);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kFalse);

  // Navigate.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_P(IndigoPageActionControllerTest,
       CheckEligibilityForCueingOptGuideFalseHeuristicTrue) {
  CreateController();
  SetupEligibleAndOnboarded();
  SetupHeuristicConfig();

  // URL in allowlist for heuristic, but OptGuide will say False.
  GURL url("https://allowed1.com/product1");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kFalse);

  FakeDocumentMetadata fake_metadata;
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata.SetResult(std::move(result));

  auto* rfh = tab_interface_->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
  auto* remote_interfaces = rfh->GetRemoteInterfaces();
  ASSERT_TRUE(remote_interfaces);

  mojo::Receiver<blink::mojom::DocumentMetadata> receiver(&fake_metadata);
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(
      blink::mojom::DocumentMetadata::Name_,
      base::BindRepeating(
          [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
             mojo::ScopedMessagePipeHandle pipe) {
            receiver->Bind(
                mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                    std::move(pipe)));
          },
          base::Unretained(&receiver)));

  // Navigate.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());

  // Expect true because heuristic says true.
  EXPECT_TRUE(future.Get());
}

TEST_P(IndigoPageActionControllerTest,
       CheckEligibilityForCueingPendingOptGuide) {
  CreateController();
  SetupEligibleAndOnboarded();

  GURL url("https://example.com");

  optimization_guide::OptimizationGuideDecisionCallback opt_guide_callback;
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::INDIGO,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(
          [&opt_guide_callback](
              const GURL& url,
              optimization_guide::proto::OptimizationType optimization_type,
              optimization_guide::OptimizationGuideDecisionCallback callback) {
            opt_guide_callback = std::move(callback);
          });

  // Navigate.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  std::move(opt_guide_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  EXPECT_TRUE(future.Get());
}

TEST_P(IndigoPageActionControllerTest,
       CheckEligibilityForCueingPendingTimeout) {
  CreateController();
  SetupEligibleAndOnboarded();

  GURL url("https://example.com");

  optimization_guide::OptimizationGuideDecisionCallback opt_guide_callback;
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          url, optimization_guide::proto::OptimizationType::INDIGO,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(
          [&opt_guide_callback](
              const GURL& url,
              optimization_guide::proto::OptimizationType optimization_type,
              optimization_guide::OptimizationGuideDecisionCallback callback) {
            opt_guide_callback = std::move(callback);
          });

  // Navigate.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  base::test::TestFuture<bool> future;
  controller_->CheckEligibilityForCueing(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(
      features::kIndigoMetadataKeywordHeuristicMaxWaitTime.Get() +
      base::Milliseconds(500));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

TEST_P(IndigoPageActionControllerTest,
       RecordsIsGlicOptedOutOnIndigoInvocation) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillRepeatedly(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  NavigateAndShowAnchoredMessage(GURL("https://example.com"));

  // 1. Entry points disabled -> Opted out
  glic::SetAnyEntryPointEnabledForTesting(profile_.get(), false);
  {
    base::HistogramTester histogram_tester;
    ClickAnchoredMessage();
    histogram_tester.ExpectUniqueSample(
        "Indigo.PageAction.IsGlicOptedOutOnIndigoInvocation", true, 1);
  }

  // 2. Entry points enabled -> Not opted out
  glic::SetAnyEntryPointEnabledForTesting(profile_.get(), true);
  {
    base::HistogramTester histogram_tester;
    ClickAnchoredMessage();
    histogram_tester.ExpectUniqueSample(
        "Indigo.PageAction.IsGlicOptedOutOnIndigoInvocation", false, 1);
  }
}

TEST_P(IndigoPageActionControllerTest, IsPageEligible) {
  CreateController();
  EXPECT_FALSE(controller_->IsPageEligible());

  GURL url1("https://example.com/eligible");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);
  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();
  EXPECT_TRUE(controller_->IsPageEligible());

  GURL url2("https://example.com/ineligible");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);
  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Commit();
  EXPECT_FALSE(controller_->IsPageEligible());
}

TEST_P(IndigoPageActionControllerTest,
       OnContextMenuShownDeduplicatedPerPageLoad) {
  CreateController();

  GURL url1("https://example.com/page1");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);
  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // First context menu shown on page1 records the impression.
  controller_->OnContextMenuShown();
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kContextMenu, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kContextMenuShowAction), 1);

  // Subsequent context menu opens on the same page load do not record again.
  controller_->OnContextMenuShown();
  controller_->OnContextMenuShown();
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kContextMenu, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kContextMenuShowAction), 1);

  // Fragment-only navigation on the same document does not reset deduplication.
  GURL url1_fragment("https://example.com/page1#section");
  auto fragment_nav = content::NavigationSimulator::CreateRendererInitiated(
      url1_fragment, tab_interface_->GetContents()->GetPrimaryMainFrame());
  fragment_nav->CommitSameDocument();
  controller_->OnContextMenuShown();
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kContextMenu, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kContextMenuShowAction), 1);

  // Navigating to a new page resets the deduplication flag.
  GURL url2("https://example.com/page2");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kTrue);
  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Commit();

  controller_->OnContextMenuShown();
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kContextMenu, 2);
  EXPECT_EQ(user_action_tester.GetActionCount(kContextMenuShowAction), 2);
}

TEST_P(IndigoPageActionControllerTest,
       InvokeActionContextMenuDoesNotOpenGlicAndRecordsMetrics) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Glic should NOT be invoked when triggered from the context menu.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  controller_->InvokeAction(EntryPoint::kContextMenu);

  EXPECT_EQ(user_action_tester.GetActionCount(kContextMenuClickAction), 1);
  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram, IndigoPageActionEntryPoint::kContextMenu, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kTransformationTriggerAction), 1);
  histogram_tester.ExpectUniqueSample(
      kTransformationTriggerSourceHistogram,
      IndigoTransformationTriggerSource::kContextMenu, 1);
}

auto IsFeaturePromo(const base::Feature& feature) {
  return testing::Truly(
      [&feature](const user_education::FeaturePromoParams& p) {
        return &p.feature.get() == &feature;
      });
}

TEST_P(IndigoPageActionControllerTest,
       ContextMenuToastPromoShownForPageActionAndRetry) {
  base::test::ScopedFeatureList local_feature_list{
      features::kIndigoContextMenu};

  CreateController();
  SetupEligibleAndOnboarded();

  testing::NiceMock<MockBrowserUserEducationInterface> mock_user_education(
      &mock_browser_window_interface_);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_interface_->GetContents(), url);
  ClickAnchoredMessage();

  EXPECT_CALL(mock_user_education,
              MaybeShowFeaturePromo(IsFeaturePromo(
                  feature_engagement::kIPHIndigoContextMenuToastFeature)))
      .Times(1);
  controller_->ShowToolbar();
  testing::Mock::VerifyAndClearExpectations(&mock_user_education);

  // Retrying a page-action transformation via the error toast also shows the
  // promo when `ShowToolbar()` is called.
  controller_->OnClose(nullptr);
  controller_->InvokeAction(EntryPoint::kErrorToast);

  EXPECT_CALL(mock_user_education,
              MaybeShowFeaturePromo(IsFeaturePromo(
                  feature_engagement::kIPHIndigoContextMenuToastFeature)))
      .Times(1);
  controller_->ShowToolbar();
  testing::Mock::VerifyAndClearExpectations(&mock_user_education);

  // Regenerating from the toolbar (or retrying that regeneration) does not
  // re-trigger the promo.
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(_)).Times(0);
  controller_->OnRegenerate(nullptr);
  controller_->ShowToolbar();
  controller_->OnClose(nullptr);
  controller_->InvokeAction(EntryPoint::kErrorToast);
  controller_->ShowToolbar();
}

TEST_P(IndigoPageActionControllerTest,
       ContextMenuToastPromoSuppressionAndNavigationEdgeCases) {
  CreateController();
  SetupEligibleAndOnboarded();

  testing::NiceMock<MockBrowserUserEducationInterface> mock_user_education(
      &mock_browser_window_interface_);

  // Not shown when `kIndigoContextMenu` is disabled.
  {
    base::test::ScopedFeatureList disabled_list;
    disabled_list.InitAndDisableFeature(features::kIndigoContextMenu);
    GURL url0("https://example.com/page0");
    ExpectOptimizationGuideDecision(url0, OptimizationGuideDecision::kTrue);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        tab_interface_->GetContents(), url0);
    ClickAnchoredMessage();

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(_)).Times(0);
    controller_->ShowToolbar();
    testing::Mock::VerifyAndClearExpectations(&mock_user_education);
  }

  base::test::ScopedFeatureList enabled_list{features::kIndigoContextMenu};

  // Not shown for a context-menu transformation or its error-toast retry.
  GURL url1("https://example.com/page1");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_interface_->GetContents(), url1);
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(_)).Times(0);
  controller_->InvokeAction(EntryPoint::kContextMenu);
  controller_->ShowToolbar();

  controller_->OnClose(nullptr);
  controller_->InvokeAction(EntryPoint::kErrorToast);
  controller_->ShowToolbar();
  testing::Mock::VerifyAndClearExpectations(&mock_user_education);

  // Not shown when the tab is hidden in the background.
  ON_CALL(*tab_interface_, IsVisible()).WillByDefault(testing::Return(false));
  ON_CALL(testing::Const(*tab_interface_), IsVisible())
      .WillByDefault(testing::Return(false));
  GURL url2("https://example.com/page2");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kTrue);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_interface_->GetContents(), url2);
  ClickAnchoredMessage();
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(_)).Times(0);
  controller_->ShowToolbar();
  testing::Mock::VerifyAndClearExpectations(&mock_user_education);

  ON_CALL(*tab_interface_, IsVisible()).WillByDefault(testing::Return(true));
  ON_CALL(testing::Const(*tab_interface_), IsVisible())
      .WillByDefault(testing::Return(true));

  // Navigating to a new page resets `last_non_retry_trigger_source_` so
  // `ShowToolbar()` on the new page does not fire without a new page-action
  // invocation.
  GURL url3("https://example.com/page3");
  ExpectOptimizationGuideDecision(url3, OptimizationGuideDecision::kTrue);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_interface_->GetContents(), url3);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(_)).Times(0);
  controller_->ShowToolbar();
}
}  // namespace
}  // namespace indigo

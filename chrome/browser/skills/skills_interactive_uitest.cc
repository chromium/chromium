// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_window_test.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                    kOpenedTabUrlState);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

static constexpr char kClickFn[] = "el => el.click()";

optimization_guide::OptimizationMetadata SkillVectorToOptimizationMetaData(
    std::vector<glic::mojom::SkillPtr> skills) {
  skills::proto::SkillsList skills_list;
  for (const auto& glicSkill : skills) {
    skills::proto::Skill* skill = skills_list.add_skills();
    skill->set_id(glicSkill->preview->id);
    skill->set_name(glicSkill->preview->name);
    skill->set_icon(glicSkill->preview->icon);
    skill->set_description(glicSkill->preview->description);
    skill->set_prompt(glicSkill->prompt);
  }
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/skills.proto.SkillsList");
  skills_list.SerializeToString(any_metadata.mutable_value());
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any_metadata);
  return metadata;
}

}  // namespace

class SkillsInteractiveUiTest
    : public TabStripInteractiveTestMixin<glic::test::InteractiveGlicTest> {
 public:
  SkillsInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicRollout,
                              features::kSkillsEnabled,
                              features::kGlicMultiInstance,
                              features::kGlicUnifiedFreScreen,
                              glic::mojom::features::kGlicMultiTab,
                              features::kGlicMultitabUnderlines},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming,
                               features::kGlicTrustFirstOnboarding});
    // Ensure that we open the FRE.
    glic_test_environment().SetFreStatusForNewProfiles(std::nullopt);
  }

  ~SkillsInteractiveUiTest() override = default;

  using StateChange = WebContentsInteractionTestUtil::StateChange;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();

    skills::SkillsServiceFactory::GetForProfile(browser()->profile())
        ->SetServiceStatusForTesting(
            skills::SkillsService::ServiceStatus::kReady);
  }

  auto UpdateContextualSkillPreviews(
      std::vector<glic::mojom::SkillPreviewPtr> contextual_skill_previews) {
    return Steps(Do([this, contextual_skill_previews =
                               std::move(contextual_skill_previews)]() mutable {
      glic_service()
          ->GetInstanceForTab(browser()->GetActiveTabInterface())
          ->host()
          .NotifyContextualSkillsChanged(std::move(contextual_skill_previews));
    }));
  }

  auto AddUserOwnedSkill(glic::mojom::SkillPtr skill,
                         std::string* out_skill_id = nullptr) {
    return Do([this, skill = std::move(skill), out_skill_id]() mutable {
      skills::SkillsService* skills_service =
          skills::SkillsServiceFactory::GetForProfile(browser()->profile());

      auto* added_skill =
          skills_service->AddSkill(skill->preview->id, skill->preview->name,
                                   skill->preview->icon, skill->prompt);

      if (out_skill_id && added_skill) {
        *out_skill_id = added_skill->id;
      }
    });
  }

  auto InvokeSkillDirectly(std::string* skill_id_ptr) {
    return Do([this, skill_id_ptr]() {
      skills::SkillsUiWindowController::From(browser())->InvokeSkill(
          *skill_id_ptr);
    });
  }

  auto VerifyInvocationInWebUI(const std::string& expected_prompt) {
    return Steps(
        Log("Verifying Glic Panel Opened via Toast Interaction"),
        WaitForShow(glic::test::kGlicHostElementId),

        // This will now pass because test_client.js updates the value!
        WaitForJsResult(
            glic::test::kGlicContentsElementId,
            base::StringPrintf(
                "() => {"
                "  const input = document.getElementById('skillPromptInput');"
                "  return !!input && input.value === '%s';"
                "}",
                expected_prompt.c_str())));
  }

  auto ActivateTabAt(int tab_index) {
    return Do([this, tab_index]() {
      browser()->GetTabStripModel()->ActivateTabAt(tab_index);
    });
  }

  auto WaitForSkillPreviewShown(std::string_view skill_name) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSkillPreviewShown);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = {"#skillsList > li > span.skill-name[value=\"" +
                          std::string(skill_name) + "\"]"};
    state_change.test_function = "el => el.checkVisibility()";
    state_change.event = kSkillPreviewShown;
    return WaitForStateChange(glic::test::kGlicContentsElementId, state_change);
  }

  auto ClickOnGlicClientElement(DeepQuery where) {
    return ExecuteJsAt(glic::test::kGlicContentsElementId, where, kClickFn);
  }

  auto PollForAndAcceptFre() {
    return Steps(
        PollUntil(
            [this]() {
              return glic_service()->fre_controller().GetWebUiState() ==
                     glic::mojom::FreWebUiState::kReady;
            },
            "polling until the fre is ready"),
        Do([this]() { glic_service()->fre_controller().AcceptFre(nullptr); }));
  }

  // Waits for the nth `tab` to be open to `url`.
  auto WaitForTabOpenedTo(int tab, GURL url) {
    return Steps(
        PollState(
            kOpenedTabUrlState,
            [this, tab]() {
              auto* const model = browser()->tab_strip_model();
              if (model->active_index() != tab) {
                return GURL();
              }
              return model->GetTabAtIndex(tab)->GetContents()->GetVisibleURL();
            }),
        WaitForState(kOpenedTabUrlState, url),
        StopObservingState(kOpenedTabUrlState));
  }

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateSkillPreviews) {
  std::vector<glic::mojom::SkillPreviewPtr> skill_previews;
  skill_previews.push_back(glic::mojom::SkillPreview::New(
      "contextual_skill_id", "contextual_skill_name", "contextual_skill_icon",
      glic::mojom::SkillSource::kFirstParty, "contextual_skill_description"));

  glic::mojom::SkillPtr skill = glic::mojom::Skill::New(
      glic::mojom::SkillPreview::New(
          "test_skill_id", "test_skill_name", "test_skill_icon",
          glic::mojom::SkillSource::kFirstParty, "test_skill_description"),
      "test_prompt", std::optional<std::string>("test_skill_id"));

  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached), PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      UpdateContextualSkillPreviews(std::move(skill_previews)),
      WaitForSkillPreviewShown("contextual_skill_name"),
      AddUserOwnedSkill(std::move(skill)),
      WaitForSkillPreviewShown("test_skill_name"));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, InvokeSkill) {
  const std::string kSkillPrompt = "Prompt from direct invocation";
  std::string generated_skill_id;

  glic::mojom::SkillPtr skill = glic::mojom::Skill::New(
      glic::mojom::SkillPreview::New("temp_id", "Direct Skill", "http://icon",
                                     glic::mojom::SkillSource::kFirstParty,
                                     "Description"),
      kSkillPrompt, std::nullopt);

  RunTestSequence(
      ToggleGlicWindow(GlicWindowMode::kAttached), PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      AddUserOwnedSkill(std::move(skill), &generated_skill_id),
      InvokeSkillDirectly(&generated_skill_id),
      VerifyInvocationInWebUI(kSkillPrompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateContextualSkill) {
  std::vector<glic::mojom::SkillPtr> contextual_skills;
  glic::mojom::SkillPtr skill = glic::mojom::Skill::New(
      glic::mojom::SkillPreview::New(
          "contextual_skill_id", "contextual_skill_name",
          "contextual_skill_icon", glic::mojom::SkillSource::kFirstParty,
          "contextual_skill_description"),
      "test_prompt", std::optional<std::string>("contextual_skill_id"));
  contextual_skills.push_back(std::move(skill));

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  optimization_guide_decider->AddHintForTesting(
      GURL("https://enabled.com/"),
      optimization_guide::proto::OptimizationType::SKILLS,
      SkillVectorToOptimizationMetaData(std::move(contextual_skills)));

  RunTestSequence(
      InstrumentTab(kFirstTabId), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      NavigateWebContents(kFirstTabId, GURL("https://enabled.com/")),
      WaitForWebContentsReady(kFirstTabId),
      WaitForSkillPreviewShown("contextual_skill_name"));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, ShowManageSkillsUi) {
  const DeepQuery kManageSkillsBtn{{"#manageSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      ClickOnGlicClientElement(kManageSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/yourSkills")),
      ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the manage skills button again and verify that it activates the
      // existing tab on chrome://skills without opening a new tab.
      ClickOnGlicClientElement(kManageSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

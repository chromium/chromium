// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_window_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

class SkillsInteractiveUiTest : public glic::test::InteractiveGlicTest {
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

  auto AddUserOwnedSkill(glic::mojom::SkillPtr skill) {
    return Steps(Do([this, skill = std::move(skill)]() mutable {
      skills::SkillsService* skills_service =
          skills::SkillsServiceFactory::GetForProfile(browser()->profile());

      skills_service->AddSkill(skill->preview->id, skill->preview->name,
                               skill->preview->icon, skill->prompt);
    }));
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

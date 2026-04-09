// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/skills/skills_interactive_uitest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "content/public/test/browser_test.h"

namespace skills {

class SkillsInteractiveUiTest : public SkillsInteractiveUiTestBase {};

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, ShowManageSkillsUi) {
  const DeepQuery kManageSkillsBtn{{"#manageSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAcceptFreAndInstrument(),
      ClickOnGlicClientElement(kManageSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/yourSkills")),
      ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the manage skills button again and verify that it activates the
      // existing tab on chrome://skills without opening a new tab.
      ClickOnGlicClientElement(kManageSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateUserSkill) {
  // Used to initially create the skill and verify the dialog
  // contents when update is triggered.
  auto user_created_skill = GetMockSkill();
  // Used to update the skill via the dialog and verify that
  // the skill was updated correctly in SkillsService.
  auto edited_skill = GetEditedSkill();
  // Used to update the skill via the API.
  std::string skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(user_created_skill, &skill_id),
      WaitForSkillPreviewShown(user_created_skill.name), UpdateSkill(&skill_id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name));

  // Verify skill was saved correctly in SkillsService.
  const auto* updated_skill = GetSkillsService()->GetSkillById(skill_id);
  ASSERT_TRUE(updated_skill);
  EXPECT_THAT(updated_skill, VerifyUserCreatedSkill(edited_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       UpdateUserSkill_NotSavedOnCancel) {
  // `user_created_skill` is used to initially create the skill and verify the
  // dialog contents when update is triggered. It is then used to verify that
  // the skill was not changed in SkillsService.
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();
  std::string skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(user_created_skill, &skill_id),
      WaitForSkillPreviewShown(user_created_skill.name), UpdateSkill(&skill_id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was NOT changed in SkillsService.
  const auto* updated_skill = GetSkillsService()->GetSkillById(skill_id);
  ASSERT_TRUE(updated_skill);
  EXPECT_THAT(updated_skill, VerifyUserCreatedSkill(user_created_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       UpdateSkillSortsByLastUpdateTime) {
  std::string updated_skill_name = "Updated Skill";
  std::string first_party_skill_name = "1P Skill";

  std::vector<Skill> test_skills;
  for (int i = 0; i <= 2; ++i) {
    auto skill = GetMockSkill();
    skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    skill.name = base::StringPrintf("Skill %d", i);
    test_skills.push_back(std::move(skill));
  }

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), Do([this, test_skills]() {
        for (const auto& skill : test_skills) {
          GetSkillsService()->AddOrUpdateSkillFromSync(
              skill.id, /*source_skill_id=*/"", skill.name, skill.icon,
              skill.prompt, skill.description,
              /*creation_time=*/base::Time::Now(),
              /*last_update_time=*/base::Time::Now(),
              sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED);
        }
      }),
      // SkillPreviews should be sorted by last update time in descending order.
      WaitForSkillPreviewOrder(
          {test_skills[2].name, test_skills[1].name, test_skills[0].name}),
      // Update oldest skill (Skill 0).
      UpdateSkill(&test_skills[0].id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      EditDialogInput(updated_skill_name, kNameInputQuery),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      // Skill 0 should now move to the top because it was just updated.
      WaitForSkillPreviewOrder(
          {updated_skill_name, test_skills[2].name, test_skills[1].name}),
      // Add a 1P skill with a creation and last update time older than all
      // other user created skills.
      Do([this, first_party_skill_name]() {
        auto older_time = base::Time::Now() - base::Minutes(10);

        GetSkillsService()->AddOrUpdateSkillFromSync(
            base::Uuid::GenerateRandomV4().AsLowercaseString(),
            /*source_skill_id=*/"", first_party_skill_name, "icon", "prompt",
            "description", /*creation_time=*/older_time,
            /*last_update_time=*/older_time,
            sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY);
      }),
      // Ensure the 1P skill is at the end of the list.
      WaitForSkillPreviewOrder({updated_skill_name, test_skills[2].name,
                                test_skills[1].name, first_party_skill_name}));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, Invoke1PSkillFromFloatyGic) {
  proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kDetached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, Invoke1PSkill) {
  proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       NotifySkillToInvokeChanged_UpdatesGetSkillToInvoke) {
  auto skill = GetMockSkill();
  std::string generated_skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(skill, &generated_skill_id),
      // Simulate a notification that the skill to invoke has changed.
      Do([this, skill, skill_id_ptr = &generated_skill_id]() mutable {
        skill.id = *skill_id_ptr;
        auto mojo_skill = glic::mojom::Skill::New(
            SkillToGlicMojomSkillPreview(&skill), skill.prompt,
            /*source_skill_id=*/std::nullopt);
        glic_service()
            ->GetInstanceForTab(browser()->GetActiveTabInterface())
            ->host()
            .NotifySkillToInvokeChanged(std::move(mojo_skill));
      }),
      // Verify the getSkillToInvoke() endpoint reflects the update.
      VerifyInvocationInWebUI(skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, InvokeCreatedSkillViaToast) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      // Wait for the "Skill saved" toast and click its action button.
      CheckToastIsShowing(ToastId::kSkillSaved),
      PressButton(toasts::ToastView::kToastActionButton),
      // Verify the action button invoked the created skill.
      VerifyInvocationInWebUI(edited_skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       UpdateAndInvokeContextualSkill) {
  auto contextual_skill = GetMockSkill();
  contextual_skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  contextual_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;

  // Add a contextual skill to the optimization guide decider.
  std::vector<glic::mojom::SkillPtr> contextual_skills;
  contextual_skills.push_back(
      glic::mojom::Skill::New(SkillToGlicMojomSkillPreview(&contextual_skill),
                              contextual_skill.prompt, std::nullopt));

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  optimization_guide_decider->AddHintForTesting(
      GURL("https://enabled.com/"),
      optimization_guide::proto::OptimizationType::SKILLS,
      SkillVectorToOptimizationMetaData(std::move(contextual_skills)));

  // Add the contextual skill to SkillsService.
  proto::Skill skill_proto = GetFirstPartySkillProto(contextual_skill);

  RunTestSequence(
      Seed1PSkills({skill_proto}), InstrumentTab(kFirstTabId),
      OpenGlicAcceptFreAndInstrument(),
      // Navigate to the site with contextual hint for `contextual_skill`.
      NavigateWebContents(kFirstTabId, GURL("https://enabled.com/")),
      WaitForWebContentsReady(kFirstTabId), WaitFor1PSkills(),
      WaitForSkillPreviewShown(contextual_skill.name),
      // Invoke the contextual skill and verify invocation WebUI.
      InvokeSkillDirectly(&contextual_skill.id),
      VerifyInvocationInWebUI(contextual_skill.prompt));
}

}  // namespace skills

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

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateSkillPreviews) {
  // Create a mock contextual 1P skill.
  auto contextual_skill = GetMockSkill();
  contextual_skill.name = "contextual_skill";
  contextual_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  std::vector<glic::mojom::SkillPreviewPtr> skill_previews;
  skill_previews.push_back(SkillToGlicMojomSkillPreview(&contextual_skill));

  // Create a mock derived skill.
  auto derived_skill = GetMockSkill();
  derived_skill.source =
      sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;

  RunTestSequence(OpenGlicAcceptFreAndInstrument(),
                  UpdateContextualSkillPreviews(std::move(skill_previews)),
                  WaitForSkillPreviewShown(contextual_skill.name),
                  AddUserOwnedSkill(derived_skill),
                  WaitForSkillPreviewShown(derived_skill.name));
}

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

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, ShowBrowseSkillsUi) {
  const DeepQuery kBrowseSkillsBtn{{"#browseSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAcceptFreAndInstrument(),
      ClickOnGlicClientElement(kBrowseSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/browse")), ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the browse skills button again and verify that it activates the
      // existing tab on chrome://skills/browse without opening a new tab.
      ClickOnGlicClientElement(kBrowseSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       GetSkill_CreatedViaCreateSkill) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name));

  // Verify skill was saved correctly in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 1u);

  auto added_skill = user_skills[0].get();
  EXPECT_THAT(added_skill, VerifyUserCreatedSkill(edited_skill));

  // Verify that the created skill can be retrieved via the API and is correct.
  auto result = GetSkill(added_skill->id);
  ASSERT_OK_AND_ASSIGN(glic::mojom::SkillPtr mojo_skill, std::move(result));

  EXPECT_EQ(mojo_skill->preview->name, edited_skill.name);
  EXPECT_EQ(mojo_skill->prompt, edited_skill.prompt);
  EXPECT_EQ(mojo_skill->preview->source,
            glic::mojom::SkillSource::kUserCreated);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       CreateUserSkill_NotSavedOnCancel) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was not saved in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 0u);
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
                       RemixFirstPartySkillAndUpdateSkill) {
  // Create a first party skill with a valid UUID and source.
  auto first_party_skill = GetMockSkill();
  first_party_skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  first_party_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  // Used to edit the remixed skill in the creation dialog.
  auto edited_skill = GetEditedSkill();
  // Used to update the remixed skill in the update dialog.
  auto updated_skill = Skill(/*id=*/"", /*name=*/"Updated Skill Name",
                             /*icon=*/"⭐", /*prompt=*/"Updated Instructions",
                             /*description=*/"");

  std::string remixed_skill_id = "";

  // Add a first party skill to the service.
  GetSkillsService()->AddOrUpdateSkillFromSync(
      first_party_skill.id, /*source_skill_id=*/"", first_party_skill.name,
      first_party_skill.icon, first_party_skill.prompt,
      first_party_skill.description,
      /*creation_time=*/base::Time::Now(),
      /*last_update_time=*/base::Time::Now(), first_party_skill.source);

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(first_party_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(first_party_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name),
      Do([this, &first_party_skill, &edited_skill, &remixed_skill_id]() {
        // Verify skill was saved correctly as a derived skill in SkillsService.
        const Skill* remixed_skill = nullptr;

        for (const auto& skill : GetSkillsService()->GetSkills()) {
          if (skill->source_skill_id == first_party_skill.id) {
            remixed_skill = skill.get();
            remixed_skill_id = skill->id;
            break;
          }
        }
        ASSERT_TRUE(remixed_skill);
        EXPECT_THAT(remixed_skill, VerifyRemixedFirstPartySkill(
                                       edited_skill, first_party_skill));
        ASSERT_FALSE(remixed_skill_id.empty());
      }),
      // Call UpdateSkill on the remixed skill.
      UpdateSkill(&remixed_skill_id),
      UninstrumentWebContents(kSkillsDialogElementId,
                              /*fail_if_not_instrumented=*/false),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(edited_skill, updated_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(updated_skill.name));
  // Verify skill was updated correctly in SkillsService.
  const auto* updated_remixed_skill =
      GetSkillsService()->GetSkillById(remixed_skill_id);
  ASSERT_TRUE(updated_remixed_skill);
  EXPECT_THAT(updated_remixed_skill,
              VerifyRemixedFirstPartySkill(updated_skill, first_party_skill));
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

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       DeleteAndUndoSkillUpdatesSkillPreviews) {
  auto user_created_skill = GetMockSkill();

  const DeepQuery kMenuButtonQuery =
      GetSkillCardQuery("cr-icon-button#moreButton");
  const DeepQuery kMenuDropdownQuery = GetSkillCardQuery("cr-action-menu#menu");
  const DeepQuery kDeleteButtonQuery =
      GetSkillCardQuery("cr-button#deleteButton");

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), AddUserOwnedSkill(user_created_skill),
      WaitForSkillPreviewOrder({user_created_skill.name}),
      // Navigate to the "Your Skills" page.
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GURL(chrome::kChromeUISkillsURL)
                              .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      WaitForWebContentsReady(kFirstTabId),
      // Delete the skill via the skill card.
      WaitForElementExists(kFirstTabId, kMenuButtonQuery),
      MoveMouseTo(kFirstTabId, kMenuButtonQuery), ClickMouse(),
      WaitForElementOpen(kFirstTabId, kMenuDropdownQuery),
      MoveMouseTo(kFirstTabId, kDeleteButtonQuery), ClickMouse(),
      // Verify deletion and toast is showing.
      WaitForSkillPreviewOrder({}), CheckToastIsShowing(ToastId::kSkillDeleted),
      // Undo the deletion and verify that the skill is restored.
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForSkillPreviewOrder({user_created_skill.name}));
}

}  // namespace skills

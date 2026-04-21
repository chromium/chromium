// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_interactive_uitest_base.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"

namespace skills {

class SkillsCreationInteractiveUiTest : public SkillsInteractiveUiTestBase {};

IN_PROC_BROWSER_TEST_F(SkillsCreationInteractiveUiTest, UpdateSkillPreviews) {
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

  RunTestSequence(OpenGlicAndInstrument(),
                  UpdateContextualSkillPreviews(std::move(skill_previews)),
                  WaitForSkillPreviewShown(contextual_skill.name),
                  AddUserOwnedSkill(derived_skill),
                  WaitForSkillPreviewShown(derived_skill.name));
}

IN_PROC_BROWSER_TEST_F(SkillsCreationInteractiveUiTest,
                       GetSkill_CreatedViaCreateSkill) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAndInstrument(), CreateSkill(user_created_skill),
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

IN_PROC_BROWSER_TEST_F(SkillsCreationInteractiveUiTest,
                       CreateUserSkill_NotSavedOnCancel) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was not saved in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SkillsCreationInteractiveUiTest,
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
      OpenGlicAndInstrument(), CreateSkill(first_party_skill),
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

IN_PROC_BROWSER_TEST_F(SkillsCreationInteractiveUiTest,
                       DeleteAndUndoSkillUpdatesSkillPreviews) {
  auto user_created_skill = GetMockSkill();

  const DeepQuery kMenuButtonQuery =
      GetSkillCardQuery("cr-icon-button#moreButton");
  const DeepQuery kMenuDropdownQuery = GetSkillCardQuery("cr-action-menu#menu");
  const DeepQuery kDeleteButtonQuery =
      GetSkillCardQuery("cr-button#deleteButton");

  RunTestSequence(
      OpenGlicAndInstrument(), AddUserOwnedSkill(user_created_skill),
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

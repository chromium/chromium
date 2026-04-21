// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_interactive_uitest_base.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"

namespace skills {

class SkillsUpdateInteractiveUiTest : public SkillsInteractiveUiTestBase {};

IN_PROC_BROWSER_TEST_F(SkillsUpdateInteractiveUiTest, ShowManageSkillsUi) {
  const DeepQuery kManageSkillsBtn{{"#manageSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAndInstrument(),
      ClickOnGlicClientElement(kManageSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/yourSkills")),
      ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the manage skills button again and verify that it activates the
      // existing tab on chrome://skills without opening a new tab.
      ClickOnGlicClientElement(kManageSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SkillsUpdateInteractiveUiTest, ShowBrowseSkillsUi) {
  const DeepQuery kBrowseSkillsBtn{{"#browseSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAndInstrument(),
      ClickOnGlicClientElement(kBrowseSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/browse")), ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the browse skills button again and verify that it activates the
      // existing tab on chrome://skills/browse without opening a new tab.
      ClickOnGlicClientElement(kBrowseSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SkillsUpdateInteractiveUiTest, UpdateUserSkill) {
  // Used to initially create the skill and verify the dialog
  // contents when update is triggered.
  auto user_created_skill = GetMockSkill();
  // Used to update the skill via the dialog and verify that
  // the skill was updated correctly in SkillsService.
  auto edited_skill = GetEditedSkill();
  // Used to update the skill via the API.
  std::string skill_id;

  RunTestSequence(
      OpenGlicAndInstrument(), AddUserOwnedSkill(user_created_skill, &skill_id),
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

IN_PROC_BROWSER_TEST_F(SkillsUpdateInteractiveUiTest,
                       UpdateUserSkill_NotSavedOnCancel) {
  // `user_created_skill` is used to initially create the skill and verify the
  // dialog contents when update is triggered. It is then used to verify that
  // the skill was not changed in SkillsService.
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();
  std::string skill_id;

  RunTestSequence(
      OpenGlicAndInstrument(), AddUserOwnedSkill(user_created_skill, &skill_id),
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

IN_PROC_BROWSER_TEST_F(SkillsUpdateInteractiveUiTest,
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
      OpenGlicAndInstrument(), Do([this, test_skills]() {
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

}  // namespace skills

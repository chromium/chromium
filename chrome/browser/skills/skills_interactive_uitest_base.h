// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_INTERACTIVE_UITEST_BASE_H_
#define CHROME_BROWSER_SKILLS_SKILLS_INTERACTIVE_UITEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/skills/skills_functional_browsertest.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/skills/public/skill.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"

namespace skills {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kElementOpen);

void PrintTo(const Skill* skill, std::ostream* os);

class SkillsInteractiveUiTestBase
    : public TabStripInteractiveTestMixin<
          skills::SkillsFunctionalBrowserTestBase> {
 public:
  SkillsInteractiveUiTestBase();
  ~SkillsInteractiveUiTestBase() override;

  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
  using StateChange = WebContentsInteractionTestUtil::StateChange;

  // Waits for a DOM element to exist.
  ui::test::InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element);

  // Waits for a DOM element to exist and not have a 'disabled' attribute.
  ui::test::InteractiveTestApi::MultiStep WaitForElementEnabled(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element);

  // Waits for a DOM element to exist and have its 'open' property set to true.
  ui::test::InteractiveTestApi::MultiStep WaitForElementOpen(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element);

  // Polls the UI until a Toast with the given `toast_id` is visibly shown.
  ui::test::InteractiveTestApi::MultiStep CheckToastIsShowing(ToastId toast_id);

  void SetUpOnMainThread() override;

  std::unique_ptr<KeyedService> CreateSkillsService(
      content::BrowserContext* context);

  // Notifies listeners that contextual skills have changed.
  ui::test::InteractiveTestApi::MultiStep UpdateContextualSkillPreviews(
      std::vector<glic::mojom::SkillPreviewPtr> contextual_skill_previews);

  // Adds a user-created skill to the SkillsService. If no skill is provided, a
  // mock skill is used. Optional `out_skill_id` stores the new skill's ID.
  ui::test::InteractiveTestApi::StepBuilder AddUserOwnedSkill(
      std::optional<skills::Skill> skill = std::nullopt,
      std::string* out_skill_id = nullptr);

  // Directly invokes a skill through the SkillsUiWindowController using its ID.
  ui::test::InteractiveTestApi::StepBuilder InvokeSkillDirectly(
      std::string* skill_id_ptr);

  // Sends a request to create a new skill via the test client.
  ui::test::InteractiveTestApi::StepBuilder CreateSkill(skills::Skill skill);

  // Sends a request to update an existing skill via the test client.
  ui::test::InteractiveTestApi::StepBuilder UpdateSkill(
      const std::string* skill_id_ptr);

  // Verifies the skill invocation prompt in the test client matches
  // `expected_prompt`.
  ui::test::InteractiveTestApi::MultiStep VerifyInvocationInWebUI(
      const std::string& expected_prompt);

  // Edits the value of an input field in the Skills Dialog at `query` to
  // `value`.
  ui::test::InteractiveTestApi::StepBuilder EditDialogInput(
      const std::string& value,
      const DeepQuery& query);

  // Verifies that the value of an input field in the Skills Dialog at `query`
  // matches `expected_value`.
  ui::test::InteractiveTestApi::MultiStep VerifyDialogInput(
      const std::string& expected_value,
      const DeepQuery& query);

  // Verifies multiple fields in the Skills Dialog using `verify_input` and
  // edits them to match `edit_input`.
  ui::test::InteractiveTestApi::MultiStep VerifyAndEditSkillDialogInput(
      const skills::Skill& verify_input,
      const skills::Skill& edit_input);

  // Clicks a button in the Skills Dialog and waits for the dialog view to hide.
  ui::test::InteractiveTestApi::MultiStep ClickButtonAndVerifyDialogHides(
      const DeepQuery& button_query);

  // Activates a specific tab in the browser's tab strip.
  ui::test::InteractiveTestApi::StepBuilder ActivateTabAt(int tab_index);

  // Waits for a SkillPreview with `skill_name` to be shown in the test client's
  // Skills list (Observes the getSkillPreviews() API endpoint).
  ui::test::InteractiveTestApi::MultiStep WaitForSkillPreviewShown(
      std::string_view skill_name);

  // Waits for the SkillPreviews in the test client's Skills list to be in the
  // same order as the given skill names.
  ui::test::InteractiveTestApi::MultiStep WaitForSkillPreviewOrder(
      const std::vector<std::string>& skill_names);

  // Clicks on the element at `where` in the test client.
  ui::test::InteractiveTestApi::StepBuilder ClickOnGlicClientElement(
      DeepQuery where);

  // Opens the Glic window, accepts the FRE, and instruments it for WebContents
  // interaction.
  ui::test::InteractiveTestApi::MultiStep OpenGlicAndInstrument();

  // Seeds a list of First-Party skills into the SkillsService.
  ui::test::InteractiveTestApi::StepBuilder Seed1PSkills(
      const std::vector<skills::proto::Skill>& skills);

  // Polls until First-Party skills have been loaded into the SkillsService.
  ui::test::InteractiveTestApi::MultiStep WaitFor1PSkills();

  // Polls until the specified tab index navigates to `url`.
  ui::test::InteractiveTestApi::MultiStep WaitForTabOpenedTo(int tab, GURL url);

 protected:
  skills::Skill GetMockSkill();
  skills::Skill GetEditedSkill();
  skills::proto::Skill GetFirstPartySkillProto(
      std::optional<skills::Skill> skill_to_convert = std::nullopt);

  const DeepQuery kNameInputQuery{"skills-dialog-app", "cr-input#nameText"};
  const DeepQuery kDescriptionInputQuery{"skills-dialog-app",
                                         "textarea#instructionsText"};
  const DeepQuery kEmojiInputQuery{"skills-dialog-app", "input#emojiTrigger"};
  const DeepQuery kSaveButtonQuery{"skills-dialog-app", "cr-button#saveButton"};
  const DeepQuery kCancelButtonQuery{"skills-dialog-app",
                                     "cr-button#cancelButton"};

  DeepQuery GetSkillCardQuery(const std::string& sub_element);

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

testing::Matcher<const Skill*> VerifyUserCreatedSkill(const Skill& expected);

testing::Matcher<const Skill*> VerifyRemixedFirstPartySkill(
    const Skill& expected,
    const Skill& source);

optimization_guide::OptimizationMetadata SkillVectorToOptimizationMetaData(
    std::vector<glic::mojom::SkillPtr> skills);

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_INTERACTIVE_UITEST_BASE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/skills/skills_interactive_uitest_base.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/chrome_features.h"
#include "components/skills/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

namespace skills {

class SkillsInvocationInteractiveUiTest : public SkillsInteractiveUiTestBase {
 public:
  void InvokeWithAutoSubmitHelper(glic::GlicInvokeOptions options) {
    glic_service()->InvokeWithAutoSubmit(
        glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
        std::move(options));
  }
};

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTest,
                       Invoke1PSkillFromFloatyGic) {
  proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kDetached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTest, Invoke1PSkill) {
  proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTest,
                       InvokeWithAutoSubmit_UpdatesWebUI) {
  auto skill = GetMockSkill();
  std::string generated_skill_id;

  RunTestSequence(
      OpenGlicAndInstrument(), AddUserOwnedSkill(skill, &generated_skill_id),
      // Simulate an invocation with auto-submit.
      Do([this, skill, skill_id_ptr = &generated_skill_id]() mutable {
        skill.id = *skill_id_ptr;
        auto mojo_skills_payload = glic::mojom::SkillsPayload::New();
        mojo_skills_payload->skill_id = skill.id;
        mojo_skills_payload->skill_name = skill.name;
        mojo_skills_payload->skill_icon = skill.icon;
        auto payload = glic::mojom::InvocationPayload::NewSkillsPayload(
            std::move(mojo_skills_payload));
        glic::GlicInvokeOptions options(
            glic::Target(*browser()->GetActiveTabInterface()),
            std::move(payload));
        options.prompts.push_back(skill.prompt);
        // Deprecated fallbacks for coverage/backwards compatibility check
        options.skill_id = skill.id;

        InvokeWithAutoSubmitHelper(std::move(options));
      }),
      // Verify the WebUI reflects the update.
      VerifyInvocationInWebUI(skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTest,
                       InvokeCreatedSkillViaToast) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAndInstrument(), CreateSkill(user_created_skill),
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

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTest,
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
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          browser()->GetProfile());
  optimization_guide_decider->AddHintForTesting(
      GURL("https://enabled.com/"),
      optimization_guide::proto::OptimizationType::SKILLS,
      SkillVectorToOptimizationMetaData(std::move(contextual_skills)));

  // Add the contextual skill to SkillsService.
  proto::Skill skill_proto = GetFirstPartySkillProto(contextual_skill);

  RunTestSequence(
      Seed1PSkills({skill_proto}), InstrumentTab(kFirstTabId),
      OpenGlicAndInstrument(),
      // Navigate to the site with contextual hint for `contextual_skill`.
      NavigateWebContents(kFirstTabId, GURL("https://enabled.com/")),
      WaitForWebContentsReady(kFirstTabId), WaitFor1PSkills(),
      WaitForSkillPreviewShown(contextual_skill.name),
      // Invoke the contextual skill and verify invocation WebUI.
      InvokeSkillDirectly(&contextual_skill.id),
      VerifyInvocationInWebUI(contextual_skill.prompt));
}

class SkillsInvocationInteractiveUiTestV2 : public SkillsInteractiveUiTestBase {
 public:
  SkillsInvocationInteractiveUiTestV2() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSkillsWebViewV2Enabled);
  }

  ui::test::InteractiveTestApi::StepBuilder InvokeSkillDirectlyV2(
      const std::string& skill_id,
      const std::string& skill_name,
      const std::string& skill_icon) {
    return Do([this, skill_id, skill_name, skill_icon]() {
      if (auto* active_tab = browser()->GetActiveTabInterface()) {
        if (auto* tab_controller =
                skills::SkillsUiTabControllerInterface::From(active_tab)) {
          tab_controller->InvokeSkill(skill_id, skill_name, skill_icon);
        }
      }
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SkillsInvocationInteractiveUiTestV2, InvokeSkillV2) {
  auto skill = GetMockSkill();
  skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  RunTestSequence(
      OpenGlicAndInstrument(),
      InvokeSkillDirectlyV2(skill.id, skill.name, skill.icon),
      VerifyInvocationInWebUI(/*expected_prompt=*/"", skill.name, skill.icon));
}

}  // namespace skills

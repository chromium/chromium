// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_apc_scrim_manager.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_password_change_run_display.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using DomUpdateCallback =
    autofill_assistant::ExternalActionDelegate::DomUpdateCallback;
using autofill_assistant::password_change::FlowType;
using autofill_assistant::password_change::ProgressStep;
using autofill_assistant::password_change::TopIcon;

namespace {

constexpr char kTitle[] = "Sample title";
constexpr char kAccessibilityTitle[] = "Accessibility sample title";
constexpr char kDescription[] = "Sample description";
constexpr char kPromptOutputKey[] = "external_output_key";
constexpr char kPromptText1[] = "Choice 1";
constexpr char kPromptText2[] = "Choice 2";
constexpr bool kIsHighlighted1 = true;
constexpr bool kIsHighlighted2 = false;
constexpr char kPromptTag1[] = "first_tag";
constexpr char kPromptTag2[] = "second_tag";
constexpr char16_t kPassword[] = u"verySecretPassword123";
constexpr TopIcon kTopIcon = TopIcon::TOP_ICON_ENTER_OLD_PASSWORD;
constexpr ProgressStep kStep = ProgressStep::PROGRESS_STEP_START;

constexpr char16_t kInterruptTitle[] = u"Title during interrupt";
constexpr char16_t kInterruptDescription[] = u"Description during interrupt";

constexpr char kUrl[] = "https://www.example.com";

autofill_assistant::external::ElementConditionsUpdate CreateDomUpdate(
    const std::vector<std::pair<int, bool>>& updates) {
  autofill_assistant::external::ElementConditionsUpdate proto;
  for (const auto& [id, satisfied] : updates) {
    auto* result = proto.add_results();
    result->set_id(id);
    result->set_satisfied(satisfied);
  }
  return proto;
}

// Helper function to create a sample proto for a base prompt.
autofill_assistant::password_change::BasePromptSpecification
CreateBasePrompt() {
  autofill_assistant::password_change::BasePromptSpecification proto;
  proto.set_output_key(kPromptOutputKey);

  proto.set_title(kTitle);

  auto* choice = proto.add_choices();
  choice->set_text(kPromptText1);
  choice->set_highlighted(kIsHighlighted1);
  choice->set_tag(kPromptTag1);

  choice = proto.add_choices();
  choice->set_text(kPromptText2);
  choice->set_highlighted(kIsHighlighted2);
  choice->set_tag(kPromptTag2);

  return proto;
}

// Helper function to create a sample proto for a generated password prompt.
autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification
CreateUseGeneratedPasswordPrompt() {
  autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification
      proto;

  proto.set_title(kTitle);
  proto.set_description(kDescription);

  auto* choice = proto.mutable_manual_password_choice();
  choice->set_text(kPromptText1);
  choice->set_highlighted(false);

  choice = proto.mutable_generated_password_choice();
  choice->set_text(kPromptText2);
  choice->set_highlighted(true);

  return proto;
}

// Helper function that creates an `Action` from a `BasePromptSpecification`.
autofill_assistant::external::Action CreateAction(
    const autofill_assistant::password_change::BasePromptSpecification& proto) {
  autofill_assistant::external::Action action;
  autofill_assistant::password_change::GenericPasswordChangeSpecification spec;
  *spec.mutable_base_prompt() = proto;
  *action.mutable_info()->mutable_generic_password_change_specification() =
      spec;

  return action;
}

// Helper function that creates an `Action` from a
// `UseGeneratedPasswordPromptSpecification`.
autofill_assistant::external::Action CreateAction(
    const autofill_assistant::password_change::
        UseGeneratedPasswordPromptSpecification& proto) {
  autofill_assistant::external::Action action;
  autofill_assistant::password_change::GenericPasswordChangeSpecification spec;
  *spec.mutable_use_generated_password_prompt() = proto;
  *action.mutable_info()->mutable_generic_password_change_specification() =
      spec;

  return action;
}

autofill_assistant::external::Action CreateAction(
    const autofill_assistant::password_change::UpdateSidePanelSpecification&
        proto) {
  autofill_assistant::external::Action action;
  autofill_assistant::password_change::GenericPasswordChangeSpecification spec;
  *spec.mutable_update_side_panel() = proto;
  *action.mutable_info()->mutable_generic_password_change_specification() =
      spec;

  return action;
}

autofill_assistant::external::Action CreateAction(
    const autofill_assistant::password_change::SetFlowTypeSpecification&
        proto) {
  autofill_assistant::external::Action action;
  autofill_assistant::password_change::GenericPasswordChangeSpecification spec;
  *spec.mutable_set_flow_type() = proto;
  *action.mutable_info()->mutable_generic_password_change_specification() =
      spec;

  return action;
}

}  // namespace

class ApcExternalActionDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    action_delegate_ = std::make_unique<ApcExternalActionDelegate>(
        web_contents(), display_delegate(), apc_scrim_manager(),
        website_login_manager());

    EXPECT_CALL(*display(), Show);
    action_delegate()->Show(display()->GetWeakPtr());
  }

  MockAssistantDisplayDelegate* display_delegate() {
    return &display_delegate_;
  }

  MockApcScrimManager* apc_scrim_manager() { return &apc_scrim_manager_; }

  autofill_assistant::MockWebsiteLoginManager* website_login_manager() {
    return &website_login_manager_;
  }

  MockPasswordChangeRunDisplay* display() { return &display_; }

  ApcExternalActionDelegate* action_delegate() {
    return action_delegate_.get();
  }

 private:
  // Supporting objects for testing.
  MockAssistantDisplayDelegate display_delegate_;
  MockPasswordChangeRunDisplay display_;
  MockApcScrimManager apc_scrim_manager_;
  autofill_assistant::MockWebsiteLoginManager website_login_manager_;

  // The object to be tested.
  std::unique_ptr<ApcExternalActionDelegate> action_delegate_;
};

TEST_F(ApcExternalActionDelegateTest, StartAndFinishInterrupt) {
  // Simulate state prior to the interrupt.
  action_delegate()->SetTitle(
      base::UTF8ToUTF16(base::StringPiece(kTitle)),
      base::UTF8ToUTF16(base::StringPiece(kAccessibilityTitle)));
  action_delegate()->SetDescription(
      base::UTF8ToUTF16(base::StringPiece(kDescription)));
  action_delegate()->SetTopIcon(kTopIcon);
  action_delegate()->SetProgressBarStep(kStep);

  // The interrupt clears model state apart from the progress step.
  EXPECT_CALL(*display(), SetTitle(std::u16string(), std::u16string()));
  EXPECT_CALL(*display(), SetDescription(std::u16string()));
  action_delegate()->OnInterruptStarted();

  // Simulate calls during interrupt.
  EXPECT_CALL(*display(),
              SetTitle(std::u16string(kInterruptTitle), std::u16string()));
  EXPECT_CALL(*display(),
              SetDescription(std::u16string(kInterruptDescription)));
  action_delegate()->SetTitle(kInterruptTitle);
  action_delegate()->SetDescription(kInterruptDescription);

  // Expect the state to be restored when the interrupt finishes.
  EXPECT_CALL(
      *display(),
      SetTitle(base::UTF8ToUTF16(base::StringPiece(kTitle)),
               base::UTF8ToUTF16(base::StringPiece(kAccessibilityTitle))));
  EXPECT_CALL(
      *display(),
      SetDescription(base::UTF8ToUTF16(base::StringPiece(kDescription))));
  EXPECT_CALL(*display(), SetTopIcon(kTopIcon));

  action_delegate()->OnInterruptFinished();
}

TEST_F(ApcExternalActionDelegateTest, OnTouchableAreaChangedShowAndHideScrim) {
  autofill_assistant::RectF visual_viewport;
  std::vector<autofill_assistant::RectF> touchable_areas;
  std::vector<autofill_assistant::RectF> restricted_areas;

  // Hides the scrim when `touchable_areas` is not empty.
  touchable_areas.emplace_back();
  EXPECT_CALL(*apc_scrim_manager(), Hide);
  EXPECT_CALL(*display(), PauseProgressBarAnimation);
  EXPECT_CALL(*display(), SetFocus);
  action_delegate()->OnTouchableAreaChanged(visual_viewport, touchable_areas,
                                            restricted_areas);

  // Shows the scrim when `touchable_areas` is not empty.
  touchable_areas.clear();
  EXPECT_CALL(*apc_scrim_manager(), Show);
  EXPECT_CALL(*display(), ResumeProgressBarAnimation);
  action_delegate()->OnTouchableAreaChanged(visual_viewport, touchable_areas,
                                            restricted_areas);
}

TEST_F(ApcExternalActionDelegateTest, ShowStartingScreen) {
  const GURL url(kUrl);

  EXPECT_CALL(*display(), ShowStartingScreen(url));
  action_delegate()->ShowStartingScreen(url);
}

TEST_F(ApcExternalActionDelegateTest, ShowCompletionScreen) {
  base::RepeatingClosure show_completion_screen_callback;
  EXPECT_CALL(*display(),
              ShowCompletionScreen(FlowType::FLOW_TYPE_UNSPECIFIED,
                                   show_completion_screen_callback));

  action_delegate()->ShowCompletionScreen(show_completion_screen_callback);
}

TEST_F(ApcExternalActionDelegateTest, ShowErrorScreen) {
  EXPECT_CALL(*display(), ShowErrorScreen());
  action_delegate()->ShowErrorScreen();
}

TEST_F(ApcExternalActionDelegateTest, PasswordWasSuccessfullyChanged) {
  base::RepeatingClosure show_completion_screen_callback;

  // Returns true if the progress step is at the end.
  ON_CALL(*display(), GetProgressStep())
      .WillByDefault(Return(autofill_assistant::password_change::ProgressStep::
                                PROGRESS_STEP_END));
  EXPECT_TRUE(action_delegate()->PasswordWasSuccessfullyChanged());

  // Returns false otherwise.
  ON_CALL(*display(), GetProgressStep())
      .WillByDefault(Return(autofill_assistant::password_change::ProgressStep::
                                PROGRESS_STEP_SAVE_PASSWORD));
  EXPECT_FALSE(action_delegate()->PasswordWasSuccessfullyChanged());
}

TEST_F(ApcExternalActionDelegateTest, ReceiveInvalidAction) {
  autofill_assistant::external::Action empty_action;

  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));
  EXPECT_FALSE(result.has_success());

  // DOM checks are never started.
  EXPECT_CALL(start_dom_checks_callback, Run).Times(0);

  action_delegate()->OnActionRequested(empty_action, /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());
  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(ApcExternalActionDelegateTest, ReceiveBasePromptAction_FromViewClick) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  // Save prompt arguments for inspection.
  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  EXPECT_CALL(*display(), ShowBasePrompt(_)).WillOnce(SaveArg<0>(&choices));

  // Similarly, save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks are always started.
  EXPECT_CALL(start_dom_checks_callback, Run);

  autofill_assistant::password_change::BasePromptSpecification proto =
      CreateBasePrompt();
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  // The view should now be set up.
  ASSERT_EQ(static_cast<size_t>(proto.choices_size()), choices.size());
  for (size_t i = 0; i < choices.size(); ++i) {
    EXPECT_EQ(choices[i].highlighted, proto.choices()[i].highlighted());
    EXPECT_EQ(choices[i].text, base::UTF8ToUTF16(proto.choices()[i].text()));
  }

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After simulating a click ...
  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnBasePromptChoiceSelected(0);

  // ... there is now a result.
  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.has_result_info());
  EXPECT_TRUE(
      result.result_info().has_generic_password_change_specification_result());
  EXPECT_TRUE(result.result_info()
                  .generic_password_change_specification_result()
                  .has_base_prompt_result());

  autofill_assistant::password_change::BasePromptSpecification::Result
      prompt_result;
  prompt_result = result.result_info()
                      .generic_password_change_specification_result()
                      .base_prompt_result();

  EXPECT_TRUE(prompt_result.has_selected_tag());
  EXPECT_EQ(prompt_result.selected_tag(), kPromptTag1);
}

TEST_F(ApcExternalActionDelegateTest,
       ReceiveBasePromptAction_FromDomCondition) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  EXPECT_CALL(*display(), ShowBasePrompt(_));

  // Save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks are started.
  DomUpdateCallback dom_update_callback;
  EXPECT_CALL(start_dom_checks_callback, Run)
      .WillOnce(SaveArg<0>(&dom_update_callback));

  autofill_assistant::password_change::BasePromptSpecification proto =
      CreateBasePrompt();
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After receiving a valid DOM condition ...
  EXPECT_CALL(*display(), ClearPrompt);
  dom_update_callback.Run(CreateDomUpdate({{1, true}, {0, true}}));

  // ... there is now a result.
  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.has_result_info());
  EXPECT_TRUE(
      result.result_info().has_generic_password_change_specification_result());
  EXPECT_TRUE(result.result_info()
                  .generic_password_change_specification_result()
                  .has_base_prompt_result());

  autofill_assistant::password_change::BasePromptSpecification::Result
      prompt_result;
  prompt_result = result.result_info()
                      .generic_password_change_specification_result()
                      .base_prompt_result();

  EXPECT_TRUE(prompt_result.has_selected_tag());
  // The result with index 0 is selected even though the arguments of the
  // DomUpdateCallback were not ordered.
  EXPECT_EQ(prompt_result.selected_tag(), kPromptTag1);
}

TEST_F(ApcExternalActionDelegateTest,
       ReceiveBasePromptAction_FailOnInvalidDomCondition) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  EXPECT_CALL(*display(), ShowBasePrompt(_));

  // Save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks are started.
  DomUpdateCallback dom_update_callback;
  EXPECT_CALL(start_dom_checks_callback, Run)
      .WillOnce(SaveArg<0>(&dom_update_callback));

  autofill_assistant::password_change::BasePromptSpecification proto =
      CreateBasePrompt();
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After receiving an invalid DOM condition ...
  dom_update_callback.Run(CreateDomUpdate({{-1, true}, {0, true}}));

  // ... the action fails.
  EXPECT_TRUE(result.has_success());
  EXPECT_FALSE(result.success());
}

TEST_F(ApcExternalActionDelegateTest,
       ReceiveBasePromptAction_FromViewClickWithoutResultKey) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  // Save prompt arguments for inspection.
  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  EXPECT_CALL(*display(), ShowBasePrompt(_)).WillOnce(SaveArg<0>(&choices));

  // Similarly, save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks are started.
  EXPECT_CALL(start_dom_checks_callback, Run);

  autofill_assistant::password_change::BasePromptSpecification proto =
      CreateBasePrompt();
  // Remove the output key.
  proto.clear_output_key();
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  // The view should now be set up.
  EXPECT_EQ(static_cast<size_t>(proto.choices_size()), choices.size());

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After simulating a click ...
  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnBasePromptChoiceSelected(0);

  // ... there is a result, but no payload.
  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_FALSE(result.has_result_info());
}

TEST_F(ApcExternalActionDelegateTest,
       ReceiveUseGeneratedPasswordPromptAction_GeneratedPasswordAccepted) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;
  std::string generated_password = base::UTF16ToUTF8(kPassword);
  ON_CALL(*website_login_manager(), GetGeneratedPassword())
      .WillByDefault(ReturnRef(generated_password));

  // Save prompt arguments for inspection.
  PasswordChangeRunDisplay::PromptChoice manual_choice, generated_choice;
  EXPECT_CALL(*display(),
              ShowUseGeneratedPasswordPrompt(
                  base::UTF8ToUTF16(base::StringPiece(kTitle)),
                  std::u16string(kPassword),
                  base::UTF8ToUTF16(base::StringPiece(kDescription)), _, _))
      .WillOnce(
          DoAll(SaveArg<3>(&manual_choice), SaveArg<4>(&generated_choice)));

  // Similarly, save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification
      proto = CreateUseGeneratedPasswordPrompt();
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  EXPECT_EQ(manual_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText1)));
  EXPECT_EQ(manual_choice.highlighted, false);
  EXPECT_EQ(generated_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText2)));
  EXPECT_EQ(generated_choice.highlighted, true);

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After simulating a click ...
  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnGeneratedPasswordSelected(true);

  // ... check success.
  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.has_result_info());
  EXPECT_TRUE(
      result.result_info().has_generic_password_change_specification_result());
  EXPECT_TRUE(result.result_info()
                  .generic_password_change_specification_result()
                  .has_use_generated_password_prompt_result());
  EXPECT_TRUE(result.result_info()
                  .generic_password_change_specification_result()
                  .use_generated_password_prompt_result()
                  .generated_password_accepted());
}

TEST_F(ApcExternalActionDelegateTest,
       ReceiveUseGeneratedPasswordPromptAction_ManualChoiceSelected) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;
  std::string generated_password = base::UTF16ToUTF8(kPassword);
  ON_CALL(*website_login_manager(), GetGeneratedPassword())
      .WillByDefault(ReturnRef(generated_password));

  // Save prompt arguments for inspection.
  PasswordChangeRunDisplay::PromptChoice manual_choice, generated_choice;
  EXPECT_CALL(*display(),
              ShowUseGeneratedPasswordPrompt(
                  base::UTF8ToUTF16(base::StringPiece(kTitle)),
                  std::u16string(kPassword),
                  base::UTF8ToUTF16(base::StringPiece(kDescription)), _, _))
      .WillOnce(
          DoAll(SaveArg<3>(&manual_choice), SaveArg<4>(&generated_choice)));

  // Similarly, save the prompt result.
  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification
      proto = CreateUseGeneratedPasswordPrompt();
  // Remove the output key.
  action_delegate()->OnActionRequested(CreateAction(proto),
                                       /* is_interrupt= */ false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  EXPECT_EQ(manual_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText1)));
  EXPECT_EQ(manual_choice.highlighted, false);
  EXPECT_EQ(generated_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText2)));
  EXPECT_EQ(generated_choice.highlighted, true);

  // But no result is sent yet.
  EXPECT_FALSE(result.has_success());

  // After simulating a click ...
  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnGeneratedPasswordSelected(false);

  // ... check success.
  EXPECT_TRUE(result.has_success());
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.has_result_info());
  autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification::
      Result use_generated_password_prompt_specification_result;
  use_generated_password_prompt_specification_result =
      result.result_info()
          .generic_password_change_specification_result()
          .use_generated_password_prompt_result();
  EXPECT_FALSE(use_generated_password_prompt_specification_result
                   .generated_password_accepted());
}

TEST_F(ApcExternalActionDelegateTest, ReceiveUpdateSidePanelAction) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  EXPECT_CALL(*display(), SetTopIcon(kTopIcon));
  EXPECT_CALL(*display(), SetProgressBarStep(kStep));
  EXPECT_CALL(*display(), SetDescription).Times(0);
  EXPECT_CALL(*display(), SetTitle(base::UTF8ToUTF16(base::StringPiece(kTitle)),
                                   std::u16string()));

  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks will never be started.
  EXPECT_CALL(start_dom_checks_callback, Run).Times(0);

  autofill_assistant::password_change::UpdateSidePanelSpecification
      update_side_panel_specification;
  update_side_panel_specification.set_top_icon(kTopIcon);
  update_side_panel_specification.set_progress_step(kStep);
  update_side_panel_specification.set_title(kTitle);

  action_delegate()->OnActionRequested(
      CreateAction(update_side_panel_specification),
      /* is_interrupt= */ false, start_dom_checks_callback.Get(),
      result_callback.Get());

  EXPECT_TRUE(result.success());
}

TEST_F(ApcExternalActionDelegateTest, ReceiveSetFlowTypeAction) {
  base::MockOnceCallback<void(
      const autofill_assistant::external::Result& result)>
      result_callback;
  base::MockOnceCallback<void(DomUpdateCallback)> start_dom_checks_callback;

  autofill_assistant::password_change::SetFlowTypeSpecification spec;
  spec.set_flow_type(FlowType::FLOW_TYPE_PASSWORD_RESET);

  autofill_assistant::external::Result result;
  EXPECT_CALL(result_callback, Run).WillOnce(SaveArg<0>(&result));

  // DOM checks will never be started.
  EXPECT_CALL(start_dom_checks_callback, Run).Times(0);

  action_delegate()->OnActionRequested(CreateAction(spec),
                                       /*is_interrupt=*/false,
                                       start_dom_checks_callback.Get(),
                                       result_callback.Get());

  // Check that the correct value was written into the model and is used when
  // the completion scren is supposed to be shown.
  base::RepeatingClosure show_completion_screen_callback;
  EXPECT_CALL(*display(),
              ShowCompletionScreen(FlowType::FLOW_TYPE_PASSWORD_RESET,
                                   show_completion_screen_callback));

  action_delegate()->ShowCompletionScreen(show_completion_screen_callback);
}

TEST_F(ApcExternalActionDelegateTest, PauseProgressBarAnimation) {
  EXPECT_CALL(*display(), PauseProgressBarAnimation);
  action_delegate()->PauseProgressBarAnimation();
}

TEST_F(ApcExternalActionDelegateTest, ResumeProgressBarAnimation) {
  EXPECT_CALL(*display(), ResumeProgressBarAnimation);
  action_delegate()->ResumeProgressBarAnimation();
}

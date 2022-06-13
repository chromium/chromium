// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

#include <memory>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_password_change_run_display.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SaveArg;
using ProgressStep = autofill_assistant::password_change::ProgressStep;
using TopIcon = autofill_assistant::password_change::TopIcon;

namespace {

constexpr char kTitle[] = "Sample title";
constexpr char kDescription[] = "Sample description";
constexpr char kPromptText1[] = "Choice 1";
constexpr char kPromptText2[] = "Choice 2";
constexpr bool kIsHighlighted1 = true;
constexpr bool kIsHighlighted2 = false;
constexpr char16_t kPassword[] = u"verySecretPassword123";
constexpr TopIcon kTopIcon = TopIcon::TOP_ICON_ENTER_OLD_PASSWORD;
constexpr ProgressStep kStep = ProgressStep::PROGRESS_STEP_START;

constexpr char16_t kInterruptTitle[] = u"Title during interrupt";
constexpr char16_t kInterruptDescription[] = u"Description during interrupt";

constexpr char kUrl[] = "https://wwww.example.com";

autofill_assistant::password_change::BasePromptSpecification
CreateBasePrompt() {
  autofill_assistant::password_change::BasePromptSpecification proto;

  proto.set_title(kTitle);

  auto* choice = proto.add_choices();
  choice->set_text(kPromptText1);
  choice->set_highlighted(kIsHighlighted1);

  choice = proto.add_choices();
  choice->set_text(kPromptText2);
  choice->set_highlighted(kIsHighlighted2);

  return proto;
}

autofill_assistant::password_change::GeneratedPasswordPromptSpecification
CreateGeneratedPasswordPrompt() {
  autofill_assistant::password_change::GeneratedPasswordPromptSpecification
      proto;

  proto.set_title(kTitle);
  proto.set_description(kDescription);

  auto* choice = proto.mutable_manual_password_choice();
  choice->set_text(kPromptText1);
  choice->set_highlighted(kIsHighlighted1);

  choice = proto.mutable_generated_password_choice();
  choice->set_text(kPromptText2);
  choice->set_highlighted(kIsHighlighted2);

  return proto;
}

}  // namespace

class ApcExternalActionDelegateTest : public ::testing::Test {
 public:
  ApcExternalActionDelegateTest() {
    action_delegate_ =
        std::make_unique<ApcExternalActionDelegate>(display_delegate());
  }

  void SetUp() override {
    EXPECT_CALL(*display(), Show);
    action_delegate()->Show(display()->GetWeakPtr());
  }

  MockAssistantDisplayDelegate* display_delegate() {
    return &display_delegate_;
  }

  MockPasswordChangeRunDisplay* display() { return &display_; }

  ApcExternalActionDelegate* action_delegate() {
    return action_delegate_.get();
  }

 private:
  // Supporting objects for testing.
  MockAssistantDisplayDelegate display_delegate_;
  MockPasswordChangeRunDisplay display_;

  // The object to be tested.
  std::unique_ptr<ApcExternalActionDelegate> action_delegate_;
};

TEST_F(ApcExternalActionDelegateTest, StartAndFinishInterrupt) {
  // Simulate state prior to the interrupt.
  action_delegate()->SetTitle(base::UTF8ToUTF16(base::StringPiece(kTitle)));
  action_delegate()->SetDescription(
      base::UTF8ToUTF16(base::StringPiece(kDescription)));
  action_delegate()->SetTopIcon(kTopIcon);
  action_delegate()->SetProgressBarStep(kStep);

  // The interrupt clears model state apart from the progress step.
  EXPECT_CALL(*display(), SetTitle(std::u16string()));
  EXPECT_CALL(*display(), SetDescription(std::u16string()));
  action_delegate()->OnInterruptStarted();

  // Simulate calls during interrupt.
  EXPECT_CALL(*display(), SetTitle(std::u16string(kInterruptTitle)));
  EXPECT_CALL(*display(),
              SetDescription(std::u16string(kInterruptDescription)));
  action_delegate()->SetTitle(kInterruptTitle);
  action_delegate()->SetDescription(kInterruptDescription);

  // Expect the state to be restored when the interrupt finishes.
  EXPECT_CALL(*display(),
              SetTitle(base::UTF8ToUTF16(base::StringPiece(kTitle))));
  EXPECT_CALL(
      *display(),
      SetDescription(base::UTF8ToUTF16(base::StringPiece(kDescription))));
  EXPECT_CALL(*display(), SetTopIcon(kTopIcon));

  action_delegate()->OnInterruptFinished();
}

TEST_F(ApcExternalActionDelegateTest, ShowBasePromptAndAccept) {
  // Save arguments for inspection.
  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  EXPECT_CALL(*display(), ShowBasePrompt).WillOnce(SaveArg<0>(&choices));
  autofill_assistant::password_change::BasePromptSpecification proto =
      CreateBasePrompt();
  action_delegate()->ShowBasePrompt(proto);

  ASSERT_EQ(static_cast<size_t>(proto.choices_size()), choices.size());
  for (size_t i = 0; i < choices.size(); ++i) {
    EXPECT_EQ(choices[i].highlighted, proto.choices()[i].highlighted());
    EXPECT_EQ(choices[i].text, base::UTF8ToUTF16(proto.choices()[i].text()));
  }

  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnBasePromptChoiceSelected(0);
}

TEST_F(ApcExternalActionDelegateTest, ShowGeneratedPasswordPromptAndAccept) {
  PasswordChangeRunDisplay::PromptChoice manual_choice, generated_choice;
  EXPECT_CALL(*display(),
              ShowGeneratedPasswordPrompt(
                  base::UTF8ToUTF16(base::StringPiece(kTitle)),
                  std::u16string(kPassword),
                  base::UTF8ToUTF16(base::StringPiece(kDescription)), _, _))
      .WillOnce(
          DoAll(SaveArg<3>(&manual_choice), SaveArg<4>(&generated_choice)));
  autofill_assistant::password_change::GeneratedPasswordPromptSpecification
      proto = CreateGeneratedPasswordPrompt();
  action_delegate()->ShowGeneratedPasswordPrompt(proto, kPassword);
  EXPECT_EQ(manual_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText1)));
  EXPECT_EQ(manual_choice.highlighted, kIsHighlighted1);
  EXPECT_EQ(generated_choice.text,
            base::UTF8ToUTF16(base::StringPiece(kPromptText2)));
  EXPECT_EQ(generated_choice.highlighted, kIsHighlighted2);

  EXPECT_CALL(*display(), ClearPrompt);
  action_delegate()->OnGeneratedPasswordSelected(true);
}

TEST_F(ApcExternalActionDelegateTest, ShowStartingScreen) {
  const GURL url(kUrl);

  EXPECT_CALL(*display(), SetTopIcon(TopIcon::TOP_ICON_UNSPECIFIED));
  EXPECT_CALL(*display(),
              SetProgressBarStep(ProgressStep::PROGRESS_STEP_START));
  EXPECT_CALL(*display(),
              SetTitle(l10n_util::GetStringFUTF16(
                  IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_STARTING_SCREEN_TITLE,
                  base::UTF8ToUTF16(url.host_piece()))));
  EXPECT_CALL(*display(), SetDescription(std::u16string()));

  action_delegate()->ShowStartingScreen(url);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_
#define ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"

namespace views {
class Textfield;
class View;
}  // namespace views

namespace ash {

class AssistantController;
class AssistantInteractionController;
class TestAssistantService;
class AssistantTestApi;

// Helper class to make testing the Assistant Ash UI easier.
class AssistantAshTestBase : public AshTestBase {
 public:
  AssistantAshTestBase();
  ~AssistantAshTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // Show the Assistant UI. The optional |entry_point| can be used to emulate
  // the different ways of launching the Assistant.
  void ShowAssistantUi(
      AssistantEntryPoint entry_point = AssistantEntryPoint::kUnspecified);
  // Close the Assistant UI without closing the launcher. The optional
  // |exit_point| can be used to emulate the different ways of closing the
  // Assistant.
  void CloseAssistantUi(
      AssistantExitPoint exit_point = AssistantExitPoint::kUnspecified);
  // Close the Assistant UI by closing the launcher.
  void CloseLauncher();

  void SetTabletMode(bool enable);

  // Change the user setting controlling whether the user prefers voice or
  // keyboard.
  void SetPreferVoice(bool value);

  // Return the actual displayed Assistant main view.
  // Can only be used after |ShowAssistantUi| has been called.
  views::View* main_view();

  // This is the top-level Assistant specific view.
  // Can only be used after |ShowAssistantUi| has been called.
  views::View* page_view();

  // Spoof sending a request to the Assistant service,
  // and receiving |response_text| as a response to display.
  void MockAssistantInteractionWithResponse(const std::string& response_text);

  // Simulate the user entering a query followed by <return>.
  void SendQueryThroughTextField(const std::string& query);

  // Simulate the user tapping on the text field.
  void TapOnTextField();

  // Create a new App window, and activate it. This will take the focus away
  // from the Assistant UI (and force it to close).
  // Returns a pointer to the newly created window.
  // The window will be destroyed when the test if finished.
  aura::Window* SwitchToNewAppWindow();

  // Return the window containing the Assistant UI.
  // Note that this window is shared for all components of the |AppList|.
  aura::Window* window();

  // Return the text field used for inputting new queries.
  views::Textfield* input_text_field();

  // Return the mic field used for dictating new queries.
  views::View* mic_view();

  // Return the greeting label shown when you first open the Assistant.
  views::View* greeting_label();

  // Show the on-screen keyboard.
  void ShowKeyboard();
  // Returns if the on-screen keyboard is being displayed.
  bool IsKeyboardShowing() const;

 private:
  AssistantInteractionController* interaction_controller();
  TestAssistantService* assistant_service();

  std::unique_ptr<AssistantTestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AssistantController* controller_ = nullptr;

  std::vector<std::unique_ptr<aura::Window>> windows_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAshTestBase);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_

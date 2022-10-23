// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/undo_window.h"

#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  ~MockAssistiveDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const override {}
};

class UndoWindowTest : public ChromeViewsTestBase {
 public:
  UndoWindowTest() {}

  UndoWindowTest(const UndoWindowTest&) = delete;
  UndoWindowTest& operator=(const UndoWindowTest&) = delete;

  ~UndoWindowTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    undo_window_ = new UndoWindow(GetContext(), delegate_.get());
    undo_button_.id = ButtonId::kUndo;
    undo_window_->InitWidget();
  }

  void TearDown() override {
    undo_window_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  UndoWindow* undo_window_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  AssistiveWindowButton undo_button_;
};

TEST_F(UndoWindowTest, HighlightsUndoButtonWhenNotHighlighted) {
  undo_window_->Show();
  undo_window_->SetButtonHighlighted(undo_button_, true);

  EXPECT_TRUE(undo_window_->GetUndoButtonForTesting()->background() != nullptr);
}

TEST_F(UndoWindowTest, KeepsHighlightingUndoButtonWhenAlreadyHighlighted) {
  undo_window_->Show();
  undo_window_->SetButtonHighlighted(undo_button_, true);
  undo_window_->SetButtonHighlighted(undo_button_, true);

  EXPECT_TRUE(undo_window_->GetUndoButtonForTesting()->background() != nullptr);
}

TEST_F(UndoWindowTest, UnhighlightsUndoButtonWhenHighlighted) {
  undo_window_->Show();
  undo_window_->SetButtonHighlighted(undo_button_, true);
  undo_window_->SetButtonHighlighted(undo_button_, false);

  EXPECT_TRUE(undo_window_->GetUndoButtonForTesting()->background() == nullptr);
}

TEST_F(UndoWindowTest,
       UnhighlightsKeepUndoButtonUnhighlightedWhenAlreadyNotHighlighted) {
  undo_window_->Show();
  undo_window_->SetButtonHighlighted(undo_button_, false);
  undo_window_->SetButtonHighlighted(undo_button_, false);

  EXPECT_TRUE(undo_window_->GetUndoButtonForTesting()->background() == nullptr);
}

}  // namespace ime
}  // namespace ui

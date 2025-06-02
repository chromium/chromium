// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/mock_quick_insert_client.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/views/quick_insert_feature_tour.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/rtl.h"
#include "base/strings/strcat.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class MockInputMethodManagerWithKeyboard
    : public input_method::MockInputMethodManager {
 public:
  input_method::ImeKeyboard* GetImeKeyboard() override { return &keyboard_; }

 private:
  input_method::FakeImeKeyboard keyboard_;
};

class QuickInsertPixelTest : public AshTestBase,
                             public testing::WithParamInterface</*RTL*/ bool> {
 public:
  QuickInsertPixelTest() {
    input_method::InputMethodManager::Initialize(
        new MockInputMethodManagerWithKeyboard);

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    QuickInsertController::DisableFeatureTourForTesting();
  }

  ~QuickInsertPixelTest() { input_method::InputMethodManager::Shutdown(); }

  void SetUp() override {
    AshTestBase::SetUp();

    ON_CALL(client_, GetHistoryService)
        .WillByDefault(Return(history_service_.get()));

    controller_ = std::make_unique<QuickInsertController>();
    controller_->SetClient(&client_);
  }

  void TearDown() override {
    controller_ = nullptr;

    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams params;
    params.under_rtl = GetParam();
    return params;
  }

  std::string GetTestName(std::string_view scenario_name) const {
    if (GetParam()) {
      return base::StrCat({scenario_name, "_rtl"});
    } else {
      return std::string(scenario_name);
    }
  }

  QuickInsertController& controller() { return *controller_; }

 private:
  NiceMock<MockQuickInsertClient> client_;
  std::unique_ptr<QuickInsertController> controller_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
};

ui::InputMethod* GetInputMethod() {
  return Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
}

INSTANTIATE_TEST_SUITE_P(, QuickInsertPixelTest, testing::Bool());

TEST_P(QuickInsertPixelTest, UnfocusedMode) {
  controller().ToggleWidget();
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "quick_insert_unfocused",
      /*revision_number=*/1, controller().widget_for_testing()));
}

TEST_P(QuickInsertPixelTest, FocusedModeNoSelection) {
  ui::FakeTextInputClient input_field(GetInputMethod(),
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();
  controller().ToggleWidget();
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "quick_insert_focused_no_selection",
      /*revision_number=*/2, controller().widget_for_testing()));
}

TEST_P(QuickInsertPixelTest, FocusedModeHasSelection) {
  ui::FakeTextInputClient input_field(GetInputMethod(),
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"abcd", gfx::Range(0, 4));
  input_field.Focus();
  controller().ToggleWidget();
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "quick_insert_focused_no_selection",
      /*revision_number=*/1, controller().widget_for_testing()));
}

}  // namespace
}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/touch_to_fill/touch_to_fill_keyboard_suppressor.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;

namespace autofill {
namespace {

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  using TestBrowserAutofillManager::TestBrowserAutofillManager;
  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), (override));
};

class TouchToFillKeyboardSuppressorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TouchToFillKeyboardSuppressorTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    suppressor_ = std::make_unique<TouchToFillKeyboardSuppressor>(
        &autofill_client(),
        base::BindRepeating(&TouchToFillKeyboardSuppressorTest::IsShowing,
                            base::Unretained(this)),
        base::BindRepeating(&TouchToFillKeyboardSuppressorTest::IntendsToShow,
                            base::Unretained(this)),
        base::Seconds(1));
    NavigateAndCommit(GURL("about:blank"));
    // Creates a second frame and AutofillManager.
    child_rfh_ = content::RenderFrameHostTester::For(
                     web_contents()->GetPrimaryMainFrame())
                     ->AppendChild("child");
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL("about:blank"), child_rfh_);
    // Forces creation of the child frame's AutofillManager.
    autofill_client().GetAutofillDriverFactory()->DriverForFrame(child_rfh_);
    ASSERT_TRUE(&autofill_manager());
    ASSERT_TRUE(&child_autofill_manager());
    ASSERT_NE(&autofill_manager(), &child_autofill_manager());
  }

  void TearDown() override {
    // The unsuppress timer isn't active.
    EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(_)).Times(0);
    EXPECT_CALL(child_autofill_manager(), SetShouldSuppressKeyboard(_))
        .Times(0);
    FastForwardBy(base::Hours(1));
    // The destructor may do a final SetShouldSuppressKeyboard().
    size_t num_calls = 0;
    EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false))
        .Times(AtMost(1))
        .WillOnce([&](auto) { ++num_calls; });
    EXPECT_CALL(child_autofill_manager(), SetShouldSuppressKeyboard(false))
        .Times(AtMost(1))
        .WillOnce([&](auto) { ++num_calls; });
    suppressor_.reset();
    EXPECT_LE(num_calls, 1u);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestContentAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  MockBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  MockBrowserAutofillManager& child_autofill_manager() {
    return *autofill_manager_injector_[child_rfh_];
  }

  void OnBeforeAskForValuesToFill(AutofillManager& manager) {
    suppressor().OnBeforeAskForValuesToFill(manager, some_form_, some_field_);
  }

  void OnAfterAskForValuesToFill(AutofillManager& manager) {
    suppressor().OnAfterAskForValuesToFill(manager, some_form_, some_field_);
  }

  void FastForwardBy(base::TimeDelta parsing_duration) {
    task_environment()->FastForwardBy(parsing_duration);
    task_environment()->RunUntilIdle();
  }

  TouchToFillKeyboardSuppressor& suppressor() { return *suppressor_; }

  void DestroySuppressor() { suppressor_.reset(); }

  MOCK_METHOD(bool, IsShowing, (AutofillManager & manager));
  MOCK_METHOD(bool,
              IntendsToShow,
              (AutofillManager&, FormGlobalId, FieldGlobalId));

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<MockBrowserAutofillManager>
      autofill_manager_injector_;
  std::unique_ptr<TouchToFillKeyboardSuppressor> suppressor_;

  FormGlobalId some_form_ = test::MakeFormGlobalId();
  FieldGlobalId some_field_ = test::MakeFieldGlobalId();

  raw_ptr<content::RenderFrameHost> child_rfh_ = nullptr;
};

// Tests that the keyboard is and remains suppressed if TTF is intended to be
// and then is shown.
TEST_F(TouchToFillKeyboardSuppressorTest, SuppressIfWillShow) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false)).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the keyboard is and remains suppressed if TTF is already showing.
TEST_F(TouchToFillKeyboardSuppressorTest, SuppressIfAlreadyShown) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false)).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the keyboard is suppressed in at most one frame at a  time.
TEST_F(TouchToFillKeyboardSuppressorTest, SuppressOnlyInOneFrame) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false)).Times(0);
  EXPECT_CALL(child_autofill_manager(), SetShouldSuppressKeyboard).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  OnAfterAskForValuesToFill(autofill_manager());
  // Now the same in another frame.
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false));
  EXPECT_CALL(child_autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(child_autofill_manager(), SetShouldSuppressKeyboard(false))
      .Times(0);
  OnBeforeAskForValuesToFill(child_autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  OnAfterAskForValuesToFill(child_autofill_manager());
}

// Tests that the keyboard is not suppressed if TTF isn't and won't be shown.
TEST_F(TouchToFillKeyboardSuppressorTest, NotSuppressIfWillNotShow) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(false));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(_)).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the keyboard is first suppressed but then unsuppressed if TTF is
// planned to be shown but not shown after all.
TEST_F(TouchToFillKeyboardSuppressorTest, UnsuppressIfNotShowing) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the keyboard is first suppressed but then unsuppressed if too much
// period of time passes between On{Before,After}AskForValuesToFill().
TEST_F(TouchToFillKeyboardSuppressorTest, UnsuppressIfParsingIsTooSlow) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false));
  FastForwardBy(base::Seconds(2));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(_)).Times(0);
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the keyboard is remains suppressed if a sufficiently short period
// of time passes between On{Before,After}AskForValuesToFill().
TEST_F(TouchToFillKeyboardSuppressorTest,
       KeepSuppressingIfParsingTakesShortTime) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false)).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(true));
  FastForwardBy(base::Milliseconds(150));
  OnAfterAskForValuesToFill(autofill_manager());
}

// Tests that the destructor unsuppresses the keyboard.
TEST_F(TouchToFillKeyboardSuppressorTest,
       UnsuppressOnDestructionIfSuppressing) {
  EXPECT_CALL(*this, IsShowing).WillOnce(Return(false));
  EXPECT_CALL(*this, IntendsToShow).WillOnce(Return(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(true));
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false)).Times(0);
  OnBeforeAskForValuesToFill(autofill_manager());
  EXPECT_CALL(autofill_manager(), SetShouldSuppressKeyboard(false));
  DestroySuppressor();
}

}  // namespace
}  // namespace autofill

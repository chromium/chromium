// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_controller_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_autofill_delegate.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::Return;

class MockTouchToFillAutofillView : public TouchToFillAutofillView {
 public:
  MockTouchToFillAutofillView() {
    ON_CALL(*this, ShowPersonalContextNotice).WillByDefault(Return(true));
  }
  ~MockTouchToFillAutofillView() override = default;

  MOCK_METHOD(bool,
              ShowPersonalContextNotice,
              (TouchToFillAutofillController * controller),
              (override));
  MOCK_METHOD(void, Hide, (), (override));
};

class MockTouchToFillAutofillDelegate : public TouchToFillAutofillDelegate {
 public:
  explicit MockTouchToFillAutofillDelegate() = default;
  ~MockTouchToFillAutofillDelegate() override = default;

  base::WeakPtr<MockTouchToFillAutofillDelegate> GetWeakPointer() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(bool,
              IntendsToShowTouchToFill,
              (FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(bool,
              TryToShowTouchToFill,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void, HideTouchToFill, (), (override));
  MOCK_METHOD(void, OnShow, (), (override));
  MOCK_METHOD(void, OnNoticeAcknowledged, (), (override));
  MOCK_METHOD(void, OnDismissed, (), (override));

 private:
  base::WeakPtrFactory<MockTouchToFillAutofillDelegate> weak_factory_{this};
};

class TestContentAutofillClientWithTouchToFillAutofillController
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  TouchToFillAutofillControllerImpl& autofill_controller() {
    return autofill_controller_;
  }

 private:
  TouchToFillAutofillControllerImpl autofill_controller_{this};
};

class TouchToFillAutofillControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillAutofillControllerImplTest() = default;
  ~TouchToFillAutofillControllerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));
    auto delegate =
        std::make_unique<testing::NiceMock<MockTouchToFillAutofillDelegate>>();
    delegate_ = delegate.get();
    autofill_manager().set_touch_to_fill_autofill_delegate(std::move(delegate));
    mock_view_ = std::make_unique<MockTouchToFillAutofillView>();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestContentAutofillClientWithTouchToFillAutofillController&
  autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  TouchToFillAutofillControllerImpl& autofill_controller() {
    return autofill_client().autofill_controller();
  }

  MockTouchToFillAutofillDelegate& ttf_delegate() { return *delegate_; }

  std::unique_ptr<MockTouchToFillAutofillView> mock_view_;

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<
      TestContentAutofillClientWithTouchToFillAutofillController>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  FormData some_form_data_ = autofill::test::CreateTestCreditCardFormData(
      /*is_https=*/true,
      /*use_month_type=*/false);
  FormGlobalId some_form_ = some_form_data_.global_id();
  FieldGlobalId some_field_ = some_form_data_.fields()[0].global_id();
  raw_ptr<MockTouchToFillAutofillDelegate> delegate_;
};

TEST_F(TouchToFillAutofillControllerImplTest, ShowNoticePassesToTheView) {
  EXPECT_CALL(ttf_delegate(), IsShowingTouchToFill)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(ttf_delegate(), IntendsToShowTouchToFill).WillOnce(Return(true));

  autofill_manager().OnAskForValuesToFill(
      some_form_data_, some_field_, gfx::Rect(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      std::nullopt);

  EXPECT_FALSE(
      autofill_controller().keyboard_suppressor_for_test().is_suppressing());

  EXPECT_CALL(*mock_view_, ShowPersonalContextNotice);
  EXPECT_TRUE(autofill_controller().ShowPersonalContextNotice(
      std::move(mock_view_), ttf_delegate().GetWeakPointer()));
}

}  // namespace
}  // namespace autofill

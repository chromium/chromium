// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class MockAutofillClient : public TestAutofillClient {
 public:
  using TestAutofillClient::TestAutofillClient;
  MOCK_METHOD(bool,
              ShouldShowPersonalContextAmbientAutofillNotice,
              (),
              (const, override));
  MOCK_METHOD(void,
              MarkPersonalContextAmbientAutofillNoticeAsAcknowledged,
              (),
              (override));
};

class TouchToFillAutofillDelegateAndroidImplTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<NiceMock<MockAutofillClient>> {
 protected:
  TouchToFillAutofillDelegateAndroidImplTest() = default;
  ~TouchToFillAutofillDelegateAndroidImplTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
    delegate_ = std::make_unique<TouchToFillAutofillDelegateAndroidImpl>(
        &autofill_manager());
  }

  void TearDown() override {
    delegate_.reset();
    DestroyAutofillClient();
  }

  TouchToFillAutofillDelegateAndroidImpl& delegate() { return *delegate_; }

  // Creates a test form, calls `AutofillManager::OnFormsSeen` with it and
  // returns a pair of the form's id and its first field's id.
  std::pair<FormGlobalId, FieldGlobalId> SeeForm() {
    const FormData form = test::CreateTestPersonalInformationFormData();
    autofill_manager().AddSeenForm(
        form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
    return {form.global_id(), form.fields()[0].global_id()};
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<TouchToFillAutofillDelegateAndroidImpl> delegate_;
};

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       IntendsToShowTouchToFillWhenClientShouldShow) {
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
  auto [form_id, field_id] = SeeForm();
  EXPECT_TRUE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       DoesNotIntendToShowTouchToFillWhenClientShouldNotShow) {
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(false));
  auto [form_id, field_id] = SeeForm();
  EXPECT_FALSE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnNoticeAcknowledgedNotifiesClient) {
  EXPECT_CALL(autofill_client(),
              MarkPersonalContextAmbientAutofillNoticeAsAcknowledged);
  delegate().OnNoticeAcknowledged();
}

}  // namespace
}  // namespace autofill

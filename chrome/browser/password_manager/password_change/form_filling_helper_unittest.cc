// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/form_filling_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/typing_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Return;

namespace {

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  using autofill::TestAutofillDriver::TestAutofillDriver;

  MOCK_METHOD(void,
              ExtractFormWithField,
              (autofill::FieldGlobalId, BrowserFormHandler callback),
              (override));
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(autofill::AutofillDriver*,
              GetAutofillDriver,
              (),
              (override, const));
};

class FormFillingHelperTest
    : public ChromeRenderViewHostTestHarness,
      public autofill::WithTestAutofillClientDriverManager<
          autofill::TestAutofillClient,
          MockAutofillDriver> {
 public:
  FormFillingHelperTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitAutofillClient();
    CreateAutofillDriver();
  }

  void TearDown() override {
    DeleteAllAutofillDrivers();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockPasswordManagerDriver* driver() { return &password_manager_driver_; }

  ChromePasswordManagerClient* GetClient() {
    return ChromePasswordManagerClient::FromWebContents(web_contents());
  }

  autofill::FormFieldData CreateField(autofill::FieldRendererId renderer_id) {
    autofill::FormFieldData field;
    field.set_host_frame(autofill::test::MakeLocalFrameToken());
    field.set_renderer_id(renderer_id);
    return field;
  }

 protected:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  MockPasswordManagerDriver password_manager_driver_;
};

TEST_F(FormFillingHelperTest, SuccessfulFilling) {
  autofill::FormFieldData field = CreateField(autofill::FieldRendererId(1));

  FormFillingHelper::FillingTasks tasks;
  tasks[field.global_id()] = u"qwerty123!";

  base::test::TestFuture<const std::optional<autofill::FormData>&>
      completion_future;
  FormFillingHelper helper(web_contents(), driver()->AsWeakPtr(),
                           std::move(tasks), completion_future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil(
      [&helper]() { return helper.typing_helper() != nullptr; }));

  // Expect a call to driver to extract the form.
  EXPECT_CALL(*driver(), GetAutofillDriver)
      .WillOnce(Return(&autofill_driver()));
  autofill::FormData extracted_form;
  extracted_form.set_fields({field});
  EXPECT_CALL(autofill_driver(), ExtractFormWithField(field.global_id(), _))
      .WillOnce(base::test::RunOnceCallback<1>(nullptr, extracted_form));

  ASSERT_TRUE(helper.typing_helper());
  helper.typing_helper()->SimulateTypingResult(true);

  EXPECT_EQ(std::optional(extracted_form), completion_future.Get());
}

TEST_F(FormFillingHelperTest, FilledInAscendingOrder) {
  autofill::FormFieldData field_1 = CreateField(autofill::FieldRendererId(1));
  autofill::FormFieldData field_2 = CreateField(autofill::FieldRendererId(2));
  autofill::FormFieldData field_3 = CreateField(autofill::FieldRendererId(3));

  FormFillingHelper::FillingTasks tasks;
  tasks[field_2.global_id()] = u"f23f@,2X8!v";
  tasks[field_1.global_id()] = u"qwerty123!";
  tasks[field_3.global_id()] = u"f23f@,2X8!v";

  base::test::TestFuture<const std::optional<autofill::FormData>&>
      completion_future;
  FormFillingHelper helper(web_contents(), driver()->AsWeakPtr(),
                           std::move(tasks), completion_future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil(
      [&helper]() { return helper.typing_helper() != nullptr; }));

  ASSERT_TRUE(helper.typing_helper());
  EXPECT_EQ(field_1.renderer_id(),
            autofill::FieldRendererId(helper.typing_helper()->dom_node_id()));
  helper.typing_helper()->SimulateTypingResult(true);

  ASSERT_TRUE(helper.typing_helper());
  EXPECT_EQ(field_2.renderer_id(),
            autofill::FieldRendererId(helper.typing_helper()->dom_node_id()));
  helper.typing_helper()->SimulateTypingResult(true);

  ASSERT_TRUE(helper.typing_helper());
  EXPECT_EQ(field_3.renderer_id(),
            autofill::FieldRendererId(helper.typing_helper()->dom_node_id()));

  // Expect a call to driver to extract the form.
  EXPECT_CALL(*driver(), GetAutofillDriver)
      .WillOnce(Return(&autofill_driver()));
  autofill::FormData extracted_form;
  extracted_form.set_fields({field_1});
  extracted_form.set_fields({field_2});
  extracted_form.set_fields({field_3});
  EXPECT_CALL(autofill_driver(), ExtractFormWithField)
      .WillOnce(base::test::RunOnceCallback<1>(nullptr, extracted_form));
  helper.typing_helper()->SimulateTypingResult(true);

  EXPECT_EQ(std::optional(extracted_form), completion_future.Get());
}

TEST_F(FormFillingHelperTest, TypingFailure) {
  autofill::FormFieldData field = CreateField(autofill::FieldRendererId(1));

  FormFillingHelper::FillingTasks tasks;
  tasks[field.global_id()] = u"qwerty123!";

  base::test::TestFuture<const std::optional<autofill::FormData>&>
      completion_future;
  FormFillingHelper helper(web_contents(), driver()->AsWeakPtr(),
                           std::move(tasks), completion_future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil(
      [&helper]() { return helper.typing_helper() != nullptr; }));

  EXPECT_CALL(*driver(), GetAutofillDriver).Times(0);
  EXPECT_CALL(autofill_driver(), ExtractFormWithField).Times(0);

  ASSERT_TRUE(helper.typing_helper());
  helper.typing_helper()->SimulateTypingResult(false);

  EXPECT_EQ(std::nullopt, completion_future.Get());
}

TEST_F(FormFillingHelperTest, FailedToObtainAutofillDriver) {
  autofill::FormFieldData field = CreateField(autofill::FieldRendererId(1));

  FormFillingHelper::FillingTasks tasks;
  tasks[field.global_id()] = u"qwerty123!";

  base::test::TestFuture<const std::optional<autofill::FormData>&>
      completion_future;
  FormFillingHelper helper(web_contents(), driver()->AsWeakPtr(),
                           std::move(tasks), completion_future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil(
      [&helper]() { return helper.typing_helper() != nullptr; }));

  EXPECT_CALL(*driver(), GetAutofillDriver).WillOnce(Return(nullptr));
  EXPECT_CALL(autofill_driver(), ExtractFormWithField).Times(0);

  ASSERT_TRUE(helper.typing_helper());
  helper.typing_helper()->SimulateTypingResult(true);

  EXPECT_EQ(std::nullopt, completion_future.Get());
}

TEST_F(FormFillingHelperTest, FormExtractionFailure) {
  autofill::FormFieldData field = CreateField(autofill::FieldRendererId(1));

  FormFillingHelper::FillingTasks tasks;
  tasks[field.global_id()] = u"qwerty123!";

  base::test::TestFuture<const std::optional<autofill::FormData>&>
      completion_future;
  FormFillingHelper helper(web_contents(), driver()->AsWeakPtr(),
                           std::move(tasks), completion_future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil(
      [&helper]() { return helper.typing_helper() != nullptr; }));

  EXPECT_CALL(*driver(), GetAutofillDriver)
      .WillOnce(Return(&autofill_driver()));
  EXPECT_CALL(autofill_driver(), ExtractFormWithField)
      .WillOnce(base::test::RunOnceCallback<1>(nullptr, std::nullopt));

  ASSERT_TRUE(helper.typing_helper());
  helper.typing_helper()->SimulateTypingResult(true);

  EXPECT_EQ(std::nullopt, completion_future.Get());
}

}  // namespace

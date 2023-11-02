// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_text_usage_logger.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace compose {
namespace {

using autofill::ContentAutofillDriver;
using autofill::FormControlType;
using autofill::FormData;
using autofill::TestAutofillClientInjector;
using autofill::TestAutofillDriverInjector;
using autofill::TestAutofillManagerInjector;
using autofill::TestBrowserAutofillManager;
using autofill::TestContentAutofillClient;
using autofill::test::CreateTestFormField;
using autofill::test::MakeFieldGlobalId;
using autofill::test::MakeFormGlobalId;
using content::RenderFrameHostImpl;
using content::RenderViewHostTestHarness;
using content::WebContents;

FormData CreateForm(
    FormControlType control_type = FormControlType::kInputText) {
  FormData form;
  form.url = GURL("https://www.foo.com");
  form.fields = {
      CreateTestFormField("Field one:", "text_value", /*value=*/"",
                          control_type),
      CreateTestFormField("Field two:", "text_value_two",
                          /*value=*/"", control_type),
      CreateTestFormField("Field three:", "text_value_three",
                          /*value=*/"", control_type),
  };
  return form;
}

class ComposeTextUsageLoggerTest : public ChromeRenderViewHostTestHarness {
 public:
  ComposeTextUsageLoggerTest() = default;
  ComposeTextUsageLoggerTest(ComposeTextUsageLoggerTest&) = delete;
  ComposeTextUsageLoggerTest& operator=(const ComposeTextUsageLoggerTest&) =
      delete;
  ~ComposeTextUsageLoggerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://a.com/"));
    ukm_source_id_ = main_rfh()->GetPageUkmSourceId();
  }

 protected:
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> LoggedTextUsage() {
    return ukm_recorder_.GetEntries(
        "Compose.TextElementUsage",
        {"AutofillFormControlType", "IsAutofillFieldType",
         "TypedCharacterCount", "TypedWordCount"});
  }
  TestBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[main_rfh()];
  }
  ComposeTextUsageLogger* logger() {
    return ComposeTextUsageLogger::GetOrCreateForCurrentDocument(main_rfh());
  }

  void SimulateTyping(autofill::FormGlobalId form_id,
                      autofill::FieldGlobalId field_id,
                      std::u16string_view text_value,
                      int start_index = 0,
                      int chars_at_a_time = 1) {
    size_t index = start_index;
    while (index < text_value.size()) {
      index = std::min(index + chars_at_a_time, text_value.size());
      logger()->OnAfterTextFieldDidChange(
          *autofill_manager(), form_id, field_id,
          std::u16string(text_value.substr(0, index)));
    }
  }

  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<ContentAutofillDriver> autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  ukm::SourceId ukm_source_id_;
};

TEST_F(ComposeTextUsageLoggerTest, TextFieldEntry) {
  FormData form_data = CreateForm(FormControlType::kInputText);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));

  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_,
                  {
                      {"AutofillFormControlType",
                       static_cast<int64_t>(FormControlType::kInputText)},
                      {"IsAutofillFieldType", 0},
                      {"TypedCharacterCount", 8},
                      {"TypedWordCount", 2},
                  })));
}

TEST_F(ComposeTextUsageLoggerTest, TextAreaEntry) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_,
                  {
                      {"AutofillFormControlType",
                       static_cast<int64_t>(FormControlType::kTextArea)},
                      {"IsAutofillFieldType", 0},
                      {"TypedCharacterCount", 8},
                      {"TypedWordCount", 2},
                  })));
}

TEST_F(ComposeTextUsageLoggerTest, FormNotFound) {
  // Not calling AddSeenFormStructure(), so the form won't be found.
  FormData form_data = CreateForm(FormControlType::kInputText);
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_, {
                                      {"AutofillFormControlType", -1},
                                      {"IsAutofillFieldType", 0},
                                      {"TypedCharacterCount", 8},
                                      {"TypedWordCount", 2},
                                  })));
}

TEST_F(ComposeTextUsageLoggerTest, SensitiveFieldEntry) {
  FormData form_data = CreateForm();
  auto form_structure = std::make_unique<autofill::FormStructure>(form_data);
  form_structure->field(0)->SetTypeTo(autofill::AutofillType(
      autofill::ServerFieldType::CREDIT_CARD_NAME_FIRST));
  autofill_manager()->AddSeenFormStructure(std::move(form_structure));

  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_,
                  {
                      {"AutofillFormControlType",
                       static_cast<int64_t>(FormControlType::kInputText)},
                      {"IsAutofillFieldType", 1},
                      {"TypedCharacterCount", -1},
                      {"TypedWordCount", -1},
                  })));
}

TEST_F(ComposeTextUsageLoggerTest, NonSensitiveAutofillFieldType) {
  FormData form_data = CreateForm();
  auto form_structure = std::make_unique<autofill::FormStructure>(form_data);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::ServerFieldType::ADDRESS_HOME_ADDRESS));
  autofill_manager()->AddSeenFormStructure(std::move(form_structure));

  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_,
                  {
                      {"AutofillFormControlType",
                       static_cast<int64_t>(FormControlType::kInputText)},
                      {"IsAutofillFieldType", 1},
                      {"TypedCharacterCount", 8},
                      {"TypedWordCount", 2},
                  })));
}

TEST_F(ComposeTextUsageLoggerTest, OnlyLastChangeIsLogged) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"One two three four");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::ElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
                  ukm_source_id_,
                  {
                      {"AutofillFormControlType",
                       static_cast<int64_t>(FormControlType::kTextArea)},
                      {"IsAutofillFieldType", 0},
                      {"TypedCharacterCount", 16 /*18 rounded down*/},
                      {"TypedWordCount", 4},
                  })));
}

TEST_F(ComposeTextUsageLoggerTest, LastChangeClearsField) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");
  logger()->OnAfterTextFieldDidChange(*autofill_manager(),
                                      form_data.global_id(),
                                      form_data.fields[0].global_id(), u"");

  DeleteContents();

  // Nothing logged.
  EXPECT_THAT(LoggedTextUsage(), testing::IsEmpty());
}

TEST_F(ComposeTextUsageLoggerTest, FieldNotEmptyAtStart) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"This is some longer text that exists in the field. New text"
                 u" is now written here !!!!",
                 /*start_index=*/50);

  DeleteContents();

  EXPECT_THAT(
      LoggedTextUsage(),
      testing::UnorderedElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
          ukm_source_id_,
          {
              {"AutofillFormControlType",
               static_cast<int64_t>(FormControlType::kTextArea)},
              {"IsAutofillFieldType", 0},
              {"TypedCharacterCount", 32},
              {"TypedWordCount", 4},
          })));
}

TEST_F(ComposeTextUsageLoggerTest, CantWriteMoreCharactersThanExistInField) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));

  // Types 0123456789 three times, replacing the field contents each time.
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"0123456789");
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"0123456789");
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"0123456789");
  DeleteContents();

  EXPECT_THAT(
      LoggedTextUsage(),
      testing::UnorderedElementsAre(ukm::TestUkmRecorder::HumanReadableUkmEntry(
          ukm_source_id_,
          {
              {"AutofillFormControlType",
               static_cast<int64_t>(FormControlType::kTextArea)},
              {"IsAutofillFieldType", 0},
              {"TypedCharacterCount", 8 /*10 rounded down*/},
              {"TypedWordCount", 1},
          })));
}

TEST_F(ComposeTextUsageLoggerTest, TwoFieldsModified) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));
  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(),
                 u"Some text");
  SimulateTyping(form_data.global_id(), form_data.fields[1].global_id(),
                 u"One two three four");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::UnorderedElementsAre(
                  ukm::TestUkmRecorder::HumanReadableUkmEntry(
                      ukm_source_id_,
                      {
                          {"AutofillFormControlType",
                           static_cast<int64_t>(FormControlType::kTextArea)},
                          {"IsAutofillFieldType", 0},
                          {"TypedCharacterCount", 8 /*9 rounded down*/},
                          {"TypedWordCount", 2},
                      }),
                  ukm::TestUkmRecorder::HumanReadableUkmEntry(
                      ukm_source_id_,
                      {
                          {"AutofillFormControlType",
                           static_cast<int64_t>(FormControlType::kTextArea)},
                          {"IsAutofillFieldType", 0},
                          {"TypedCharacterCount", 16 /*18 rounded down*/},
                          {"TypedWordCount", 4},
                      })));
}

TEST_F(ComposeTextUsageLoggerTest, CountingWordsCorrectly) {
  FormData form_data = CreateForm(FormControlType::kTextArea);
  autofill_manager()->AddSeenFormStructure(
      std::make_unique<autofill::FormStructure>(form_data));

  SimulateTyping(form_data.global_id(), form_data.fields[0].global_id(), u" ");
  SimulateTyping(form_data.global_id(), form_data.fields[1].global_id(),
                 u"\r\n hi\tmom\r");
  SimulateTyping(form_data.global_id(), form_data.fields[2].global_id(),
                 u" word");

  DeleteContents();

  EXPECT_THAT(LoggedTextUsage(),
              testing::UnorderedElementsAre(
                  ukm::TestUkmRecorder::HumanReadableUkmEntry(
                      ukm_source_id_,
                      {
                          {"AutofillFormControlType",
                           static_cast<int64_t>(FormControlType::kTextArea)},
                          {"IsAutofillFieldType", 0},
                          {"TypedCharacterCount", 1},
                          {"TypedWordCount", 0},
                      }),
                  ukm::TestUkmRecorder::HumanReadableUkmEntry(
                      ukm_source_id_,
                      {
                          {"AutofillFormControlType",
                           static_cast<int64_t>(FormControlType::kTextArea)},
                          {"IsAutofillFieldType", 0},
                          {"TypedCharacterCount", 4 /*5 rounded down*/},
                          {"TypedWordCount", 1},
                      }),
                  ukm::TestUkmRecorder::HumanReadableUkmEntry(
                      ukm_source_id_,
                      {
                          {"AutofillFormControlType",
                           static_cast<int64_t>(FormControlType::kTextArea)},
                          {"IsAutofillFieldType", 0},
                          {"TypedCharacterCount", 8 /*10 rounded down*/},
                          {"TypedWordCount", 2},
                      })));
}

}  // namespace
}  // namespace compose

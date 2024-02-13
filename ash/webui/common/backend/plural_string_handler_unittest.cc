// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/plural_string_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
constexpr char kHandlerFunctionName[] = "handlerFunctionName";

}  // namespace

class PluralStringHandlerTest : public testing::Test {
 public:
  PluralStringHandlerTest()
      : plural_string_handler_(std::make_unique<PluralStringHandler>()) {
    plural_string_handler_->SetWebUIForTest(&web_ui_);
    plural_string_handler_->RegisterMessages();
    // Add edit button label to plural map for testing purposes.
    plural_string_handler_->AddStringToPluralMap(
        "editButtonLabel", IDS_SCANNING_APP_EDIT_BUTTON_LABEL);
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *web_ui_.call_data()[index];
  }

  ~PluralStringHandlerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  const std::unique_ptr<PluralStringHandler> plural_string_handler_;
};

TEST_F(PluralStringHandlerTest, PluralString) {
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append("editButtonLabel");
  args.Append(/*count=*/2);
  web_ui_.HandleReceivedMessage("getPluralString", args);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(call_data_count_before_call + 1, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(/*success=*/call_data.arg2()->GetBool());
  EXPECT_EQ("Edit files", call_data.arg3()->GetString());
}

TEST_F(PluralStringHandlerTest, SingularString) {
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append("editButtonLabel");
  args.Append(/*count=*/1);
  web_ui_.HandleReceivedMessage("getPluralString", args);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(call_data_count_before_call + 1, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(/*success=*/call_data.arg2()->GetBool());
  EXPECT_EQ("Edit file", call_data.arg3()->GetString());
}

TEST_F(PluralStringHandlerTest, InvalidPluralStringRequest) {
  base::Value::List args;
  args.Append(kHandlerFunctionName);
  args.Append(/*name=*/"invalidKey");
  args.Append(/*count=*/2);
  web_ui_.HandleReceivedMessage("getPluralString", args);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, web_ui_.call_data().size());
}

}  // namespace ash

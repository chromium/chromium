// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/javascript/web_context_fetcher_util.h"

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chrome/browser/android/history_report/delta_file_service.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using testing::_;

namespace web_context_fetcher_util {

class WebContextFetcherUtilTest : public testing::Test {
 public:
  WebContextFetcherUtilTest() {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(WebContextFetcherUtilTest);
};

// Test that call to get category image with three site icons works.
TEST_F(WebContextFetcherUtilTest, TestConvertJavascriptOutputToValidJson) {
  std::string empty_string = "";
  EXPECT_EQ("{}", WebContextFetcherUtil::ConvertJavascriptOutputToValidJson(
                      empty_string));

  std::string empty_object = "{}";
  EXPECT_EQ("{}", WebContextFetcherUtil::ConvertJavascriptOutputToValidJson(
                      empty_object));

  std::string quoted_empty_object = "\"{}\"";
  EXPECT_EQ("{}", WebContextFetcherUtil::ConvertJavascriptOutputToValidJson(
                      quoted_empty_object));

  std::string valid_json_object = "\"{\\\"testing\\\": \\\"123\\\"}\"";
  EXPECT_EQ("{\"testing\": \"123\"}",
            WebContextFetcherUtil::ConvertJavascriptOutputToValidJson(
                valid_json_object));
}

}  // namespace web_context_fetcher_util

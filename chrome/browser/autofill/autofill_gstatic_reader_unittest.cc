// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_gstatic_reader.h"

#include <utility>

#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillGstaticReaderTest : public ::testing::Test {
 public:
  AutofillGstaticReaderTest() {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AutofillGstaticReaderTest, ParseListJSON_ValidResponseGetsParsed) {
  std::unique_ptr<std::string> response = std::make_unique<std::string>(
      "{\"list_data_key\": [\"list_item_one\", "
      "\"list_item_two\"]}");
  std::vector<std::string> merchant_whitelist =
      AutofillGstaticReader::GetInstance()->ParseListJSON(std::move(response),
                                                          "list_data_key");
  EXPECT_EQ(merchant_whitelist.size(), 2U);
  EXPECT_EQ(merchant_whitelist[0], "list_item_one");
  EXPECT_EQ(merchant_whitelist[1], "list_item_two");
}

TEST_F(AutofillGstaticReaderTest, ParseListJSON_InvalidKeyNotParsed) {
  std::unique_ptr<std::string> response = std::make_unique<std::string>(
      "{\"randomKey\": [\"list_item_one\", \"list_item_two\"]}");
  std::vector<std::string> merchant_whitelist =
      AutofillGstaticReader::GetInstance()->ParseListJSON(std::move(response),
                                                          "list_data_key");
  // "list_data_key" isn't a key in |response|, so we expect to return an empty
  // list.
  EXPECT_EQ(merchant_whitelist.size(), 0U);
}

TEST_F(AutofillGstaticReaderTest, ParseListJSON_NonStringListEntryNotParsed) {
  std::unique_ptr<std::string> response = std::make_unique<std::string>(
      "{\"list_data_key\": [1, \"list_item_two\"]}");
  std::vector<std::string> merchant_whitelist =
      AutofillGstaticReader::GetInstance()->ParseListJSON(std::move(response),
                                                          "list_data_key");
  EXPECT_EQ(merchant_whitelist.size(), 1U);
  EXPECT_EQ(merchant_whitelist[0], "list_item_two");
}

TEST_F(AutofillGstaticReaderTest, ParseListJSON_NonDictionaryNotParsed) {
  std::unique_ptr<std::string> response =
      std::make_unique<std::string>("list_item_one");
  std::vector<std::string> merchant_whitelist =
      AutofillGstaticReader::GetInstance()->ParseListJSON(std::move(response),
                                                          "list_data_key");
  EXPECT_EQ(merchant_whitelist.size(), 0U);
}

}  // namespace autofill

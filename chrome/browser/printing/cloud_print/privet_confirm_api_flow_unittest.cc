// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_confirm_api_flow.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::_;

namespace cloud_print {

namespace {

const char kSampleConfirmResponse[] = "{"
    "   \"success\": true"
    "}";

const char kFailedConfirmResponse[] = "{"
    "   \"success\": false"
    "}";

TEST(PrivetConfirmApiFlowTest, Params) {
  PrivetConfirmApiCallFlow confirmation(
      "123", PrivetConfirmApiCallFlow::ResponseCallback());
  EXPECT_EQ(GURL("https://www.google.com/cloudprint/confirm?token=123"),
            confirmation.GetURL());
  EXPECT_EQ("https://www.googleapis.com/auth/cloudprint",
            confirmation.GetOAuthScope());
  EXPECT_FALSE(confirmation.GetExtraRequestHeaders().empty());
}

class MockDelegate {
 public:
  MOCK_METHOD1(Callback, void(GCDApiFlow::Status));
};

TEST(PrivetConfirmApiFlowTest, Parsing) {
  StrictMock<MockDelegate> delegate;
  PrivetConfirmApiCallFlow confirmation(
      "123",
      base::BindOnce(&MockDelegate::Callback, base::Unretained(&delegate)));
  EXPECT_CALL(delegate, Callback(GCDApiFlow::SUCCESS)).Times(1);

  base::Optional<base::Value> value =
      base::JSONReader::Read(kSampleConfirmResponse);
  ASSERT_TRUE(value);
  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  confirmation.OnGCDApiFlowComplete(*dictionary);

  PrivetConfirmApiCallFlow confirmation2(
      "123",
      base::BindOnce(&MockDelegate::Callback, base::Unretained(&delegate)));
  EXPECT_CALL(delegate, Callback(GCDApiFlow::ERROR_FROM_SERVER)).Times(1);

  value = base::JSONReader::Read(kFailedConfirmResponse);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  confirmation2.OnGCDApiFlowComplete(*dictionary);
}

}  // namespace

}  // namespace cloud_print

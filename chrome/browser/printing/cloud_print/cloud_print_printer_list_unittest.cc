// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_printer_list.h"

#include <stddef.h>

#include <memory>
#include <set>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace cloud_print {

namespace {

const char kSampleSuccessResponseOAuth[] = "{"
    "   \"success\": true,"
    "   \"printers\": ["
    "     {\"id\" : \"someID\","
    "      \"displayName\": \"someDisplayName\","
    "      \"description\": \"someDescription\"}"
    "    ]"
    "}";

class MockDelegate : public CloudPrintPrinterList::Delegate {
 public:
  MOCK_METHOD1(OnDeviceListReady,
               void(const CloudPrintPrinterList::DeviceList&));
  MOCK_METHOD0(OnDeviceListUnavailable, void());
};

TEST(CloudPrintPrinterListTest, Params) {
  CloudPrintPrinterList device_list(NULL);
  EXPECT_EQ(GURL("https://www.google.com/cloudprint/search"),
            device_list.GetURL());
  EXPECT_EQ("https://www.googleapis.com/auth/cloudprint",
            device_list.GetOAuthScope());
  EXPECT_FALSE(device_list.GetExtraRequestHeaders().empty());
}

TEST(CloudPrintPrinterListTest, Parsing) {
  StrictMock<MockDelegate> delegate;
  CloudPrintPrinterList device_list(&delegate);
  CloudPrintPrinterList::DeviceList devices;
  EXPECT_CALL(delegate, OnDeviceListReady(_)).WillOnce(SaveArg<0>(&devices));

  absl::optional<base::Value> value =
      base::JSONReader::Read(kSampleSuccessResponseOAuth);
  ASSERT_TRUE(value);
  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  device_list.OnGCDApiFlowComplete(*dictionary);

  Mock::VerifyAndClear(&delegate);

  std::set<std::string> ids_expected;
  ids_expected.insert("someID");

  std::set<std::string> ids_found;
  for (const auto& device : devices)
    ids_found.insert(device.id);

  ASSERT_EQ(ids_expected, ids_found);
  EXPECT_EQ("someID", devices[0].id);
  EXPECT_EQ("someDisplayName", devices[0].display_name);
  EXPECT_EQ("someDescription", devices[0].description);
}

}  // namespace

}  // namespace cloud_print

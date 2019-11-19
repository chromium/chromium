// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_service.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace media_router {

// Create Test Data
namespace {

DialDeviceData CreateDialDeviceData(int num) {
  DialDeviceData device_data(
      base::StringPrintf("Device id %d", num),
      GURL(base::StringPrintf("http://192.168.1.%d/dd.xml", num)),
      base::Time::Now());
  device_data.set_label(base::StringPrintf("Device label %d", num));
  return device_data;
}

DialDeviceDescriptionData CreateDialDeviceDescriptionData(int num) {
  return DialDeviceDescriptionData(
      "", GURL(base::StringPrintf("http://192.168.1.%d/apps", num)));
}

ParsedDialDeviceDescription CreateParsedDialDeviceDescription(int num) {
  ParsedDialDeviceDescription description_data;
  description_data.app_url =
      GURL(base::StringPrintf("http://192.168.1.%d/apps", num));
  description_data.friendly_name = base::StringPrintf("Friendly name %d", num);
  description_data.model_name = base::StringPrintf("Model name %d", num);
  description_data.unique_id = base::StringPrintf("Unique ID %d", num);
  return description_data;
}

}  // namespace

class TestDeviceDescriptionService : public DeviceDescriptionService {
 public:
  TestDeviceDescriptionService(
      const DeviceDescriptionParseSuccessCallback& success_cb,
      const DeviceDescriptionParseErrorCallback& error_cb)
      : DeviceDescriptionService(success_cb, error_cb) {}

  MOCK_METHOD2(ParseDeviceDescription,
               void(const DialDeviceData&, const DialDeviceDescriptionData&));
};

class DeviceDescriptionServiceTest : public ::testing::Test {
 public:
  DeviceDescriptionServiceTest()
      : device_description_service_(mock_success_cb_.Get(),
                                    mock_error_cb_.Get()),
        fetcher_map_(
            device_description_service_.device_description_fetcher_map_),
        description_cache_(device_description_service_.description_cache_) {}

  TestDeviceDescriptionService* device_description_service() {
    return &device_description_service_;
  }

  void AddToCache(const std::string& device_label,
                  const ParsedDialDeviceDescription& description_data,
                  bool expired) {
    DeviceDescriptionService::CacheEntry cache_entry;
    cache_entry.expire_time =
        base::Time::Now() + (expired ? -1 : 1) * base::TimeDelta::FromHours(12);
    cache_entry.description_data = description_data;
    description_cache_[device_label] = cache_entry;
  }

  void OnDeviceDescriptionFetchComplete(int num) {
  }

  void TestOnParsedDeviceDescription(
      ParsedDialDeviceDescription device_description,
      SafeDialDeviceDescriptionParser::ParsingError parsing_error,
      const std::string& error_message) {
    GURL app_url("http://192.168.1.1/apps");
    auto device_data = CreateDialDeviceData(1);
    auto description_data = CreateParsedDialDeviceDescription(1);

    if (!error_message.empty()) {
      EXPECT_CALL(mock_error_cb_, Run(device_data, error_message));
    } else {
      EXPECT_CALL(mock_success_cb_, Run(device_data, description_data));
    }
    device_description_service()->OnParsedDeviceDescription(
        device_data, device_description, parsing_error);
  }

 protected:
  base::test::TaskEnvironment environment_;
  base::MockCallback<
      DeviceDescriptionService::DeviceDescriptionParseSuccessCallback>
      mock_success_cb_;
  base::MockCallback<
      DeviceDescriptionService::DeviceDescriptionParseErrorCallback>
      mock_error_cb_;

  TestDeviceDescriptionService device_description_service_;
  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>&
      fetcher_map_;
  std::map<std::string, DeviceDescriptionService::CacheEntry>&
      description_cache_;
};

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFromCache) {
  auto device_data = CreateDialDeviceData(1);
  auto description_data = CreateParsedDialDeviceDescription(1);
  EXPECT_CALL(mock_success_cb_, Run(device_data, description_data));

  AddToCache(device_data.label(), description_data, false /* expired */);

  std::vector<DialDeviceData> devices = {device_data};
  device_description_service()->GetDeviceDescriptions(devices);
}

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFetchURL) {
  DialDeviceData device_data = CreateDialDeviceData(1);
  std::vector<DialDeviceData> devices = {device_data};

  // Create Fetcher
  EXPECT_TRUE(fetcher_map_.empty());
  device_description_service()->GetDeviceDescriptions(devices);
  EXPECT_EQ(size_t(1), fetcher_map_.size());

  // Remove fetcher.
  EXPECT_CALL(*device_description_service(), ParseDeviceDescription(_, _));

  auto description_response_data = CreateDialDeviceDescriptionData(1);
  auto parsed_description_data = CreateParsedDialDeviceDescription(1);

  EXPECT_CALL(mock_success_cb_, Run(device_data, parsed_description_data));

  device_description_service_.OnDeviceDescriptionFetchComplete(
      device_data, description_response_data);
  device_description_service_.OnParsedDeviceDescription(
      device_data, CreateParsedDialDeviceDescription(1),
      SafeDialDeviceDescriptionParser::ParsingError::kNone);
}

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFetchURLError) {
  DialDeviceData device_data = CreateDialDeviceData(1);
  std::vector<DialDeviceData> devices;
  devices.push_back(device_data);

  // Create Fetcher
  EXPECT_TRUE(fetcher_map_.empty());
  device_description_service()->GetDeviceDescriptions(devices);
  EXPECT_EQ(size_t(1), fetcher_map_.size());

  EXPECT_CALL(mock_error_cb_, Run(device_data, ""));

  device_description_service()->OnDeviceDescriptionFetchError(device_data, "");
  EXPECT_TRUE(fetcher_map_.empty());
}

TEST_F(DeviceDescriptionServiceTest,
       TestGetDeviceDescriptionRemoveOutDatedFetchers) {
  DialDeviceData device_data_1 = CreateDialDeviceData(1);
  DialDeviceData device_data_2 = CreateDialDeviceData(2);
  DialDeviceData device_data_3 = CreateDialDeviceData(3);

  std::vector<DialDeviceData> devices;
  devices.push_back(device_data_1);
  devices.push_back(device_data_2);

  // insert fetchers
  device_description_service()->GetDeviceDescriptions(devices);

  // Keep fetchers non exist in current device list and remove fetchers with
  // different description url.
  GURL new_url_2 = GURL("http://example.com");
  device_data_2.set_device_description_url(new_url_2);

  devices.clear();
  devices.push_back(device_data_2);
  devices.push_back(device_data_3);
  device_description_service()->GetDeviceDescriptions(devices);

  EXPECT_EQ(size_t(3), fetcher_map_.size());

  auto* description_fetcher = fetcher_map_[device_data_2.label()].get();
  EXPECT_EQ(new_url_2, description_fetcher->device_description_url());

  EXPECT_CALL(mock_error_cb_, Run(_, _)).Times(3);
  device_description_service()->OnDeviceDescriptionFetchError(device_data_1,
                                                              "");
  device_description_service()->OnDeviceDescriptionFetchError(device_data_2,
                                                              "");
  device_description_service()->OnDeviceDescriptionFetchError(device_data_3,
                                                              "");
}

TEST_F(DeviceDescriptionServiceTest, TestCleanUpCacheEntries) {
  DialDeviceData device_data_1 = CreateDialDeviceData(1);
  DialDeviceData device_data_2 = CreateDialDeviceData(2);
  DialDeviceData device_data_3 = CreateDialDeviceData(3);

  AddToCache(device_data_1.label(), ParsedDialDeviceDescription(),
             true /* expired */);
  AddToCache(device_data_2.label(), ParsedDialDeviceDescription(),
             true /* expired */);
  AddToCache(device_data_3.label(), ParsedDialDeviceDescription(),
             false /* expired */);

  device_description_service_.CleanUpCacheEntries();
  EXPECT_EQ(size_t(1), description_cache_.size());
  EXPECT_TRUE(base::Contains(description_cache_, device_data_3.label()));

  AddToCache(device_data_3.label(), ParsedDialDeviceDescription(),
             true /* expired*/);
  device_description_service_.CleanUpCacheEntries();
  EXPECT_TRUE(description_cache_.empty());
}

TEST_F(DeviceDescriptionServiceTest, TestOnParsedDeviceDescription) {
  GURL app_url("http://192.168.1.1/apps");
  DialDeviceData device_data = CreateDialDeviceData(1);

  // XML parsing errors.
  std::string error_message = "Failed to parse device description XML";
  SafeDialDeviceDescriptionParser::ParsingError errors[] = {
      SafeDialDeviceDescriptionParser::ParsingError::kInvalidXml,
      SafeDialDeviceDescriptionParser::ParsingError::kFailedToReadUdn,
      SafeDialDeviceDescriptionParser::ParsingError::kFailedToReadFriendlyName,
      SafeDialDeviceDescriptionParser::ParsingError::kFailedToReadModelName,
      SafeDialDeviceDescriptionParser::ParsingError::kFailedToReadDeviceType};
  for (auto error : errors)
    TestOnParsedDeviceDescription(ParsedDialDeviceDescription(), error,
                                  error_message);

  // Empty field
  error_message = "Failed to process fetch result";
  TestOnParsedDeviceDescription(
      ParsedDialDeviceDescription(),
      SafeDialDeviceDescriptionParser::ParsingError::kNone, error_message);

  // Valid device description and put in cache
  auto description = CreateParsedDialDeviceDescription(1);
  TestOnParsedDeviceDescription(
      description, SafeDialDeviceDescriptionParser::ParsingError::kNone, "");
  EXPECT_EQ(size_t(1), description_cache_.size());

  // Valid device description ptr and skip cache.
  size_t cache_num = 256;
  for (size_t i = 0; i < cache_num; i++) {
    AddToCache(std::to_string(i), ParsedDialDeviceDescription(),
               false /* expired */);
  }

  EXPECT_EQ(size_t(cache_num + 1), description_cache_.size());
  description = CreateParsedDialDeviceDescription(1);
  TestOnParsedDeviceDescription(
      description, SafeDialDeviceDescriptionParser::ParsingError::kNone, "");
  EXPECT_EQ(size_t(cache_num + 1), description_cache_.size());
}

}  // namespace media_router

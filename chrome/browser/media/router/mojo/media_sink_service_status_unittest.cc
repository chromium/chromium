// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_sink_service_status.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

std::unique_ptr<base::Value> DeserializeJSONString(const std::string& str) {
  JSONStringValueDeserializer deserializer(str);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(0, error_code);
  EXPECT_TRUE(error_message.empty());
  return value;
}

// Helper function to compare two JSON strings. The two strings may have
// different format and spaces. Still returns true if their contents are the
// same.
void VerifyEqualJSONString(const std::string& expected_str,
                           const std::string& str) {
  std::unique_ptr<base::Value> expected_value =
      DeserializeJSONString(expected_str);
  std::unique_ptr<base::Value> actual_value = DeserializeJSONString(str);
  ASSERT_TRUE(expected_value);
  ASSERT_TRUE(actual_value);
  EXPECT_EQ(*expected_value, *actual_value);
}

}  // namespace

TEST(MediaSinkServiceStatusTest, TestGetStatusAsJSONStringEmptyStatus) {
  const char expected_str[] = R"(
      {
        "available_sinks": { },
        "discovered_sinks": { }
      })";

  MediaSinkServiceStatus status;
  std::string str = status.GetStatusAsJSONString();
  VerifyEqualJSONString(expected_str, str);
}

TEST(MediaSinkServiceStatusTest, TestGetStatusAsJSONStringEmptySinks) {
  const char expected_str[] = R"(
      {
        "available_sinks": {
          "DIAL:dial:youtube" : ["dial:de51", "dial:id2"]
        },
        "discovered_sinks": { }
      })";

  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  dial_sink1.sink().set_sink_id("dial:de51d94921f15f8af6dbf65592bb3610");
  MediaSinkInternal dial_sink2 = CreateDialSink(2);
  std::vector<MediaSinkInternal> available_sinks = {dial_sink1, dial_sink2};

  MediaSinkServiceStatus status;
  status.UpdateAvailableSinks(mojom::MediaRouteProviderId::DIAL, "dial:youtube",
                              available_sinks);

  std::string str = status.GetStatusAsJSONString();
  VerifyEqualJSONString(expected_str, str);
}

TEST(MediaSinkServiceStatusTest, TestGetStatusAsJSONStringEmptyAvailability) {
  const char expected_str[] = R"(
      {
        "available_sinks": { },
        "discovered_sinks": {
          "dial": [
            {
              "app_url":"http://192.168.0.101/apps",
              "icon_type":7,
              "id":"dial:id1",
              "ip_address":"192.168.0.101",
              "model_name":"model name 1",
              "name":"friendly name 1"
            },
            {
              "app_url":"http://192.168.0.102/apps",
              "icon_type":7,
              "id":"dial:id2",
              "ip_address":"192.168.0.102",
              "model_name":"model name 2",
              "name":"friendly name 2"
            }
          ]
        }
      })";

  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  MediaSinkInternal dial_sink2 = CreateDialSink(2);
  std::vector<MediaSinkInternal> discovered_sinks = {dial_sink1, dial_sink2};

  MediaSinkServiceStatus status;
  status.UpdateDiscoveredSinks("dial", discovered_sinks);

  std::string str = status.GetStatusAsJSONString();
  VerifyEqualJSONString(expected_str, str);
}

TEST(MediaSinkServiceStatusTest, TestGetStatusAsJSONStringMultipleProviders) {
  const char expected_str[] = R"(
      {
        "available_sinks": {
          "CAST:cast:netflix" : ["cast:id2"],
          "DIAL:dial:youtube" : ["dial:id1"]
        },
        "discovered_sinks": {
          "dial": [
            {
              "app_url":"http://192.168.0.101/apps",
              "icon_type":7,
              "id":"dial:id1",
              "ip_address":"192.168.0.101",
              "model_name":"model name 1",
              "name":"friendly name 1"
            }
          ],
          "cast": [
            {
              "capabilities": 5,
              "channel_id": 2,
              "discovered_by_dial": false,
              "icon_type": 0,
              "id": "cast:id2",
              "ip_endpoint": "192.168.0.102:8011",
              "model_name": "model name 2",
              "name": "friendly name 2"
            }
          ]
        }
      })";

  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  MediaSinkInternal dial_sink2 = CreateCastSink(2);
  std::vector<MediaSinkInternal> sinks1 = {dial_sink1};
  std::vector<MediaSinkInternal> sinks2 = {dial_sink2};

  MediaSinkServiceStatus status;
  status.UpdateDiscoveredSinks("dial", sinks1);
  status.UpdateDiscoveredSinks("cast", sinks2);
  status.UpdateAvailableSinks(mojom::MediaRouteProviderId::DIAL, "dial:youtube",
                              sinks1);
  status.UpdateAvailableSinks(mojom::MediaRouteProviderId::CAST, "cast:netflix",
                              sinks2);

  std::string str = status.GetStatusAsJSONString();
  VerifyEqualJSONString(expected_str, str);
}

}  // namespace media_router

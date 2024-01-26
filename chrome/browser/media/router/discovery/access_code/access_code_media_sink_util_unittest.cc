// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using testing::FieldsAre;

class AccessCodeMediaSinkUtilTest : public testing::Test {
 public:
  AccessCodeMediaSinkUtilTest(const AccessCodeMediaSinkUtilTest&
                                  access_code_media_sink_util_test) = delete;
  AccessCodeMediaSinkUtilTest& operator=(
      const AccessCodeMediaSinkUtilTest& access_code_media_sink_util_test) =
      delete;

 protected:
  AccessCodeMediaSinkUtilTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~AccessCodeMediaSinkUtilTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AccessCodeMediaSinkUtilTest, MissingDisplayName) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.set_display_name("");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingFriendlyName);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingId) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.set_id("");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingID);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingNetworkInfo) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.clear_network_info();
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingNetworkInfo);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingBothIpAddresses) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_ip_v4_address("");
  discovery_device_proto.mutable_network_info()->set_ip_v6_address("");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingOrInvalidIPAddress);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingIp4Addresses) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_ip_v4_address("");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kOk);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingIp6Addresses) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_ip_v6_address("");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kOk);
}

TEST_F(AccessCodeMediaSinkUtilTest, InvalidIp4Address) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_ip_v6_address("");
  discovery_device_proto.mutable_network_info()->set_ip_v4_address("GG123?");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingOrInvalidIPAddress);
}

TEST_F(AccessCodeMediaSinkUtilTest, InvalidIp6Address) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_ip_v4_address("");
  discovery_device_proto.mutable_network_info()->set_ip_v6_address("GG?123?");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingOrInvalidIPAddress);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingPort) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();

  media_router::MediaSinkInternal expected_sink_internal;
  media_router::CastSinkExtraData expected_extra_data;

  expected_extra_data.capabilities = {
      cast_channel::CastDeviceCapability::kVideoOut,
      cast_channel::CastDeviceCapability::kVideoIn,
      cast_channel::CastDeviceCapability::kAudioOut,
      cast_channel::CastDeviceCapability::kAudioIn,
      cast_channel::CastDeviceCapability::kDevMode};
  net::IPAddress expected_ip;

  // Must use equality to bypass `warn_unused_result`.
  EXPECT_EQ(true, expected_ip.AssignFromIPLiteral(kExpectedIpV6));

  discovery_device_proto.mutable_network_info()->clear_port();

  expected_extra_data.ip_endpoint =
      net::IPEndPoint(expected_ip, kCastControlPort);
  expected_extra_data.discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  expected_extra_data.model_name = "Chromecast Cast Moderator";

  media_router::MediaSink expected_sink(
      base::StringPrintf("cast:<%s>", kExpectedSinkId), kExpectedDisplayName,
      media_router::GetCastSinkIconType(expected_extra_data.capabilities),
      media_router::mojom::MediaRouteProviderId::CAST);

  expected_sink_internal.set_sink(expected_sink);
  expected_sink_internal.set_cast_data(expected_extra_data);

  std::pair<std::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
      constructed_pair = CreateAccessCodeMediaSink(discovery_device_proto);

  EXPECT_EQ(constructed_pair.second, CreateCastMediaSinkResult::kOk);

  EXPECT_EQ(constructed_pair.first.value(), expected_sink_internal);
}

TEST_F(AccessCodeMediaSinkUtilTest, InvalidPort) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.mutable_network_info()->set_port(
      "```````23489:1238:1239");
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingOrInvalidPort);
}

TEST_F(AccessCodeMediaSinkUtilTest, MissingDeviceCapabilities) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  discovery_device_proto.clear_device_capabilities();
  EXPECT_EQ(CreateAccessCodeMediaSink(discovery_device_proto).second,
            CreateCastMediaSinkResult::kMissingDeviceCapabilities);
}

TEST_F(AccessCodeMediaSinkUtilTest, MediaSinkCreatedCorrectly) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();

  media_router::MediaSinkInternal expected_sink_internal;
  media_router::CastSinkExtraData expected_extra_data;

  expected_extra_data.capabilities = {
      cast_channel::CastDeviceCapability::kVideoOut,
      cast_channel::CastDeviceCapability::kVideoIn,
      cast_channel::CastDeviceCapability::kAudioOut,
      cast_channel::CastDeviceCapability::kAudioIn,
      cast_channel::CastDeviceCapability::kDevMode};
  net::IPAddress expected_ip;

  // Must use equality to bypass `warn_unused_result`.
  EXPECT_EQ(true, expected_ip.AssignFromIPLiteral(kExpectedIpV6));

  int port_value = 0;
  EXPECT_EQ(true, base::StringToInt(kExpectedPort, &port_value));
  expected_extra_data.ip_endpoint = net::IPEndPoint(expected_ip, port_value);
  expected_extra_data.discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  expected_extra_data.model_name = "Chromecast Cast Moderator";

  media_router::MediaSink expected_sink(
      base::StringPrintf("cast:<%s>", kExpectedSinkId), kExpectedDisplayName,
      media_router::GetCastSinkIconType(expected_extra_data.capabilities),
      media_router::mojom::MediaRouteProviderId::CAST);

  expected_sink_internal.set_sink(expected_sink);
  expected_sink_internal.set_cast_data(expected_extra_data);

  std::pair<std::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
      constructed_pair = CreateAccessCodeMediaSink(discovery_device_proto);

  EXPECT_EQ(constructed_pair.second, CreateCastMediaSinkResult::kOk);

  EXPECT_EQ(constructed_pair.first.value(), expected_sink_internal);
}

TEST_F(AccessCodeMediaSinkUtilTest, ParsedMediaSinkInternalEqualToOriginal) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  auto cast_sink =
      CreateAccessCodeMediaSink(discovery_device_proto).first.value();
  media_router::CastSinkExtraData cast_sink_data = cast_sink.cast_data();
  cast_sink_data.discovery_type =
      CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink.set_cast_data(cast_sink_data);

  EXPECT_EQ(ParseValueDictIntoMediaSinkInternal(
                CreateValueDictFromMediaSinkInternal(cast_sink))
                .value(),
            cast_sink);
}

TEST_F(AccessCodeMediaSinkUtilTest, AddSinkResultMetricsHelper) {
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::UNKNOWN_ERROR),
            AccessCodeCastAddSinkResult::kUnknownError);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::OK),
            AccessCodeCastAddSinkResult::kOk);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::AUTH_ERROR),
            AccessCodeCastAddSinkResult::kAuthError);
  EXPECT_EQ(
      AddSinkResultMetricsHelper(AddSinkResultCode::HTTP_RESPONSE_CODE_ERROR),
      AccessCodeCastAddSinkResult::kHttpResponseCodeError);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::RESPONSE_MALFORMED),
            AccessCodeCastAddSinkResult::kResponseMalformed);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::EMPTY_RESPONSE),
            AccessCodeCastAddSinkResult::kEmptyResponse);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::INVALID_ACCESS_CODE),
            AccessCodeCastAddSinkResult::kInvalidAccessCode);
  EXPECT_EQ(
      AddSinkResultMetricsHelper(AddSinkResultCode::ACCESS_CODE_NOT_FOUND),
      AccessCodeCastAddSinkResult::kAccessCodeNotFound);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::TOO_MANY_REQUESTS),
            AccessCodeCastAddSinkResult::kTooManyRequests);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::SERVICE_NOT_PRESENT),
            AccessCodeCastAddSinkResult::kServiceNotPresent);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::SERVER_ERROR),
            AccessCodeCastAddSinkResult::kServerError);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::SINK_CREATION_ERROR),
            AccessCodeCastAddSinkResult::kSinkCreationError);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::CHANNEL_OPEN_ERROR),
            AccessCodeCastAddSinkResult::kChannelOpenError);
  EXPECT_EQ(AddSinkResultMetricsHelper(AddSinkResultCode::PROFILE_SYNC_ERROR),
            AccessCodeCastAddSinkResult::kProfileSyncError);
}

TEST_F(AccessCodeMediaSinkUtilTest, GetIPEndPointFromValueDict) {
  DiscoveryDevice discovery_device_proto = BuildDiscoveryDeviceProto();
  auto cast_sink =
      CreateAccessCodeMediaSink(discovery_device_proto).first.value();
  media_router::CastSinkExtraData cast_sink_data = cast_sink.cast_data();
  cast_sink_data.discovery_type =
      CastDiscoveryType::kAccessCodeRememberedDevice;
  cast_sink.set_cast_data(cast_sink_data);
  EXPECT_EQ(GetIPEndPointFromValueDict(
                CreateValueDictFromMediaSinkInternal(cast_sink))
                .value(),
            cast_sink.cast_data().ip_endpoint);
}

}  // namespace media_router

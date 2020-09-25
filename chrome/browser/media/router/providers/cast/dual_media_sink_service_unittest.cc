// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

#include "base/bind.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class DualMediaSinkServiceTest : public testing::Test {
 public:
  DualMediaSinkServiceTest() {
    auto cast_media_sink_service = std::make_unique<MockCastMediaSinkService>();
    auto dial_media_sink_service = std::make_unique<MockDialMediaSinkService>();
    auto cast_app_discovery_service =
        std::make_unique<MockCastAppDiscoveryService>();
    cast_media_sink_service_ = cast_media_sink_service.get();
    dial_media_sink_service_ = dial_media_sink_service.get();
    cast_app_discovery_service_ = cast_app_discovery_service.get();
    dual_media_sink_service_ = std::unique_ptr<DualMediaSinkService>(
        new DualMediaSinkService(std::move(cast_media_sink_service),
                                 std::move(dial_media_sink_service),
                                 std::move(cast_app_discovery_service)));
  }

  ~DualMediaSinkServiceTest() override = default;

  MockDialMediaSinkService* dial_media_sink_service() {
    return dial_media_sink_service_;
  }
  MockCastMediaSinkService* cast_media_sink_service() {
    return cast_media_sink_service_;
  }
  DualMediaSinkService* dual_media_sink_service() {
    return dual_media_sink_service_.get();
  }

  MOCK_METHOD2(OnSinksDiscovered,
               void(const std::string& provider_name,
                    const std::vector<MediaSinkInternal>& sinks));

 private:
  MockCastMediaSinkService* cast_media_sink_service_;
  MockDialMediaSinkService* dial_media_sink_service_;
  MockCastAppDiscoveryService* cast_app_discovery_service_;
  std::unique_ptr<DualMediaSinkService> dual_media_sink_service_;
};

TEST_F(DualMediaSinkServiceTest, OnUserGesture) {
  EXPECT_CALL(*cast_media_sink_service(), OnUserGesture());
  dual_media_sink_service()->OnUserGesture();
}

TEST_F(DualMediaSinkServiceTest, AddSinksDiscoveredCallback) {
  auto subscription = dual_media_sink_service()->AddSinksDiscoveredCallback(
      base::BindRepeating(&DualMediaSinkServiceTest::OnSinksDiscovered,
                          base::Unretained(this)));

  base::flat_map<std::string, std::vector<MediaSinkInternal>> sink_map;
  std::string dial_provider_name = "dial";
  MediaSinkInternal dial_sink1;
  dial_sink1.set_sink(MediaSink("dial_sink1", "", SinkIconType::GENERIC,
                                MediaRouteProviderId::EXTENSION));
  MediaSinkInternal dial_sink2;
  dial_sink2.set_sink(MediaSink("dial_sink2", "", SinkIconType::GENERIC,
                                MediaRouteProviderId::EXTENSION));

  sink_map[dial_provider_name] = {dial_sink1, dial_sink2};

  EXPECT_CALL(*this, OnSinksDiscovered(dial_provider_name,
                                       sink_map[dial_provider_name]));
  dual_media_sink_service()->OnSinksDiscovered(dial_provider_name,
                                               sink_map[dial_provider_name]);

  EXPECT_EQ(sink_map, dual_media_sink_service()->current_sinks());

  // |this| no longer receive updates.
  subscription.reset();

  std::string cast_provider_name = "cast";
  MediaSinkInternal cast_sink;
  cast_sink.set_sink(MediaSink("cast_sink", "", SinkIconType::CAST,
                               MediaRouteProviderId::EXTENSION));
  sink_map[cast_provider_name] = {cast_sink};
  EXPECT_CALL(*this, OnSinksDiscovered(testing::_, testing::_)).Times(0);
  dual_media_sink_service()->OnSinksDiscovered(cast_provider_name,
                                               sink_map[cast_provider_name]);

  EXPECT_EQ(sink_map, dual_media_sink_service()->current_sinks());
}

}  // namespace media_router

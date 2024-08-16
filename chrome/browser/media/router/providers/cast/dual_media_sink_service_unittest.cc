// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media_router {

namespace {

constexpr char kCastProviderName[] = "cast";
constexpr char kDialProviderName[] = "dial";

std::vector<MediaSinkInternal> CreateCastSinks() {
  MediaSinkInternal cast_sink;
  cast_sink.set_sink(MediaSink("cast_sink", "", SinkIconType::CAST,
                               mojom::MediaRouteProviderId::CAST));
  return {cast_sink};
}

std::vector<MediaSinkInternal> CreateDialSinks() {
  MediaSinkInternal dial_sink1;
  dial_sink1.set_sink(MediaSink("dial_sink1", "", SinkIconType::GENERIC,
                                mojom::MediaRouteProviderId::DIAL));
  MediaSinkInternal dial_sink2;
  dial_sink2.set_sink(MediaSink("dial_sink2", "", SinkIconType::GENERIC,
                                mojom::MediaRouteProviderId::DIAL));
  return {dial_sink1, dial_sink2};
}

}  // namespace

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

  MOCK_METHOD2(OnSinksDiscovered,
               void(const std::string& provider_name,
                    const std::vector<MediaSinkInternal>& sinks));

 protected:
  // Must outlive the raw_ptrs below.
  std::unique_ptr<DualMediaSinkService> dual_media_sink_service_;

  raw_ptr<MockCastMediaSinkService> cast_media_sink_service_ = nullptr;
  raw_ptr<MockDialMediaSinkService> dial_media_sink_service_ = nullptr;
  raw_ptr<MockCastAppDiscoveryService> cast_app_discovery_service_ = nullptr;

 private:
  content::BrowserTaskEnvironment task_environment;
};

TEST_F(DualMediaSinkServiceTest, DiscoverSinksNow) {
  EXPECT_CALL(*cast_media_sink_service_, DiscoverSinksNow());
  dual_media_sink_service_->DiscoverSinksNow();
}

TEST_F(DualMediaSinkServiceTest, AddSinksDiscoveredCallback) {
  auto subscription = dual_media_sink_service_->AddSinksDiscoveredCallback(
      base::BindRepeating(&DualMediaSinkServiceTest::OnSinksDiscovered,
                          base::Unretained(this)));
  base::flat_map<std::string, std::vector<MediaSinkInternal>> sink_map = {
      {kDialProviderName, CreateDialSinks()}};

  EXPECT_CALL(
      *this, OnSinksDiscovered(kDialProviderName, sink_map[kDialProviderName]));
  dual_media_sink_service_->OnSinksDiscovered(kDialProviderName,
                                              sink_map[kDialProviderName]);
  EXPECT_EQ(sink_map, dual_media_sink_service_->current_sinks_);

  // |this| no longer receive updates.
  subscription = {};

  sink_map[kCastProviderName] = CreateCastSinks();
  EXPECT_CALL(*this, OnSinksDiscovered(_, _)).Times(0);
  dual_media_sink_service_->OnSinksDiscovered(kCastProviderName,
                                              sink_map[kCastProviderName]);
  EXPECT_EQ(sink_map, dual_media_sink_service_->current_sinks_);
}

TEST_F(DualMediaSinkServiceTest, AddSinksDiscoveredCallbackAfterDiscovery) {
  base::flat_map<std::string, std::vector<MediaSinkInternal>> sink_map = {
      {kDialProviderName, CreateDialSinks()}};
  dual_media_sink_service_->OnSinksDiscovered(kDialProviderName,
                                              sink_map[kDialProviderName]);

  // The callback should be called even if it was added after the sinks were
  // discovered.
  EXPECT_CALL(
      *this, OnSinksDiscovered(kDialProviderName, sink_map[kDialProviderName]));
  auto subscription = dual_media_sink_service_->AddSinksDiscoveredCallback(
      base::BindRepeating(&DualMediaSinkServiceTest::OnSinksDiscovered,
                          base::Unretained(this)));
}

TEST_F(DualMediaSinkServiceTest, SetPermissionRejectedCallback) {
  base::MockCallback<base::RepeatingClosure> cb;
  dual_media_sink_service_->SetDiscoveryPermissionRejectedCallback(cb.Get());
  EXPECT_CALL(cb, Run());
  dual_media_sink_service_->OnDiscoveryPermissionRejected();
}

}  // namespace media_router

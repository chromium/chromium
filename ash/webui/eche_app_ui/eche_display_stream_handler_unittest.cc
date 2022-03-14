// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_display_stream_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {
namespace {

class FakeObserver : public EcheDisplayStreamHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_start_streaming_calls() const {
    return num_start_streaming_calls_;
  }
  mojom::StreamStatus last_notified_stream_status() const {
    return last_notified_stream_status_;
  }

  // EcheDisplayStreamHandler::Observer:
  void OnStartStreaming() override { ++num_start_streaming_calls_; }
  void OnStreamStatusChanged(mojom::StreamStatus status) override {
    last_notified_stream_status_ = status;
  }

 private:
  size_t num_start_streaming_calls_ = 0;
  mojom::StreamStatus last_notified_stream_status_ =
      mojom::StreamStatus::kStreamStatusUnknown;
};

}  // namespace

class EcheDisplayStreamHandlerTest : public testing::Test {
 protected:
  EcheDisplayStreamHandlerTest() = default;
  EcheDisplayStreamHandlerTest(const EcheDisplayStreamHandlerTest&) = delete;
  EcheDisplayStreamHandlerTest& operator=(const EcheDisplayStreamHandlerTest&) =
      delete;
  ~EcheDisplayStreamHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    handler_ = std::make_unique<EcheDisplayStreamHandler>();
    handler_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    handler_->RemoveObserver(&fake_observer_);
    handler_.reset();
  }

  void StartStreaming() { handler_->StartStreaming(); }
  void NotifyStreamStatus(mojom::StreamStatus status) {
    handler_->OnStreamStatusChanged(status);
  }

  size_t GetNumObserverStartStreamingCalls() const {
    return fake_observer_.num_start_streaming_calls();
  }
  mojom::StreamStatus GetObservedStreamStatus() const {
    return fake_observer_.last_notified_stream_status();
  }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<EcheDisplayStreamHandler> handler_;
};

TEST_F(EcheDisplayStreamHandlerTest, StartStreaming) {
  StartStreaming();
  EXPECT_EQ(1u, GetNumObserverStartStreamingCalls());
}

TEST_F(EcheDisplayStreamHandlerTest, OnStreamStatusChanged) {
  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusInitializing);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusInitializing,
            GetObservedStreamStatus());

  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusStarted);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusStarted,
            GetObservedStreamStatus());

  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusStopped);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusStopped,
            GetObservedStreamStatus());
}

}  // namespace eche_app
}  // namespace ash

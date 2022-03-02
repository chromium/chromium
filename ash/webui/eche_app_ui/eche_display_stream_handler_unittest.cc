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

  size_t num_calls() const { return num_calls_; }

  // EcheDisplayStreamHandler::Observer:
  void OnStartStreaming() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
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

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<EcheDisplayStreamHandler> handler_;
};

TEST_F(EcheDisplayStreamHandlerTest, StartStreaming) {
  StartStreaming();
  EXPECT_EQ(1u, GetNumObserverCalls());
}

}  // namespace eche_app
}  // namespace ash
// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_tracing_instance.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/trace_event.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

std::string GetTracingConfig() {
  base::trace_event::TraceConfig config("-*,exo,viz,toplevel,gpu",
                                        base::trace_event::RECORD_CONTINUOUSLY);
  config.EnableSystrace();
  return config.ToString();
}

}  // namespace

class ArcTracingBridgeTest : public testing::Test {
 public:
  ArcTracingBridgeTest() = default;
  ~ArcTracingBridgeTest() override = default;

  ArcTracingBridgeTest(const ArcTracingBridgeTest&) = delete;
  ArcTracingBridgeTest& operator=(const ArcTracingBridgeTest&) = delete;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user");

    tracing_bridge_ =
        std::make_unique<ArcTracingBridge>(profile_, &bridge_service_);
    bridge_service_.tracing()->SetInstance(&fake_tracing_instance_);
    WaitForInstanceReady(bridge_service_.tracing());
  }

  void TearDown() override {
    bridge_service_.tracing()->CloseInstance(&fake_tracing_instance_);
    tracing_bridge_.reset();
    profile_manager_.reset();
  }

 protected:
  ArcTracingBridge* tracing_bridge() { return tracing_bridge_.get(); }
  const FakeTracingInstance& tracing_instance() const {
    return fake_tracing_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  ArcBridgeService bridge_service_;
  std::unique_ptr<ArcTracingBridge> tracing_bridge_;
  FakeTracingInstance fake_tracing_instance_;
 private:
  ::base::test::TracingEnvironment tracing_environment_;
};

TEST_F(ArcTracingBridgeTest, Creation) {
  ASSERT_TRUE(tracing_bridge());
}

TEST_F(ArcTracingBridgeTest, Tracing) {
  EXPECT_EQ(ArcTracingBridge::State::kDisabled, tracing_bridge()->state());
  EXPECT_EQ(0, tracing_instance().start_count());
  EXPECT_EQ(0, tracing_instance().stop_count());
  tracing_bridge()->StartTracing(
      GetTracingConfig(),
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  EXPECT_EQ(ArcTracingBridge::State::kEnabled, tracing_bridge()->state());
  EXPECT_EQ(1, tracing_instance().start_count());
  EXPECT_EQ(0, tracing_instance().stop_count());
  tracing_bridge()->StopTracing(base::BindOnce([]() {}));
  EXPECT_EQ(ArcTracingBridge::State::kDisabled, tracing_bridge()->state());
  EXPECT_EQ(1, tracing_instance().start_count());
  EXPECT_EQ(1, tracing_instance().stop_count());
}

}  // namespace arc

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/prerewritten_event_forwarder.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class PrerewrittenEventForwarderTest : public AshTestBase {
 public:
  PrerewrittenEventForwarderTest() = default;
  PrerewrittenEventForwarderTest(const PrerewrittenEventForwarderTest&) =
      delete;
  PrerewrittenEventForwarderTest& operator=(
      const PrerewrittenEventForwarderTest&) = delete;
  ~PrerewrittenEventForwarderTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    rewriter_ = std::make_unique<PrerewrittenEventForwarder>();
  }

  void TearDown() override {
    rewriter_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PrerewrittenEventForwarder> rewriter_;
};

TEST_F(PrerewrittenEventForwarderTest, InitializationTest) {
  EXPECT_NE(rewriter_.get(), nullptr);
}

}  // namespace ash

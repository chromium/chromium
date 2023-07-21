// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class PeripheralCustomizationEventRewriterTest : public AshTestBase {
 public:
  PeripheralCustomizationEventRewriterTest() = default;
  PeripheralCustomizationEventRewriterTest(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  PeripheralCustomizationEventRewriterTest& operator=(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  ~PeripheralCustomizationEventRewriterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    rewriter_ = std::make_unique<PeripheralCustomizationEventRewriter>();
  }

  void TearDown() override {
    rewriter_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PeripheralCustomizationEventRewriter> rewriter_;
};

TEST_F(PeripheralCustomizationEventRewriterTest, InitializationTest) {
  EXPECT_NE(rewriter_.get(), nullptr);
}

}  // namespace ash

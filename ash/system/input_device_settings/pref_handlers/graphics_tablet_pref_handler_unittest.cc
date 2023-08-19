// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class GraphicsTabletPrefHandlerTest : public AshTestBase {
 public:
  GraphicsTabletPrefHandlerTest() = default;
  GraphicsTabletPrefHandlerTest(const GraphicsTabletPrefHandlerTest&) = delete;
  GraphicsTabletPrefHandlerTest& operator=(
      const GraphicsTabletPrefHandlerTest&) = delete;
  ~GraphicsTabletPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    pref_handler_ = std::make_unique<GraphicsTabletPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<GraphicsTabletPrefHandlerImpl> pref_handler_;
};

TEST_F(GraphicsTabletPrefHandlerTest, InitializationTest) {
  EXPECT_NE(pref_handler_.get(), nullptr);
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_accelerator_configuration.h"

#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace {

class BrowserAcceleratorConfigurationTest : public AshTestBase {
 public:
  BrowserAcceleratorConfigurationTest() = default;
  BrowserAcceleratorConfigurationTest(
      const BrowserAcceleratorConfigurationTest&) = delete;
  BrowserAcceleratorConfigurationTest& operator=(
      const BrowserAcceleratorConfigurationTest&) = delete;
  ~BrowserAcceleratorConfigurationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    config_ = std::make_unique<BrowserAcceleratorConfiguration>();
  }

 protected:
  std::unique_ptr<BrowserAcceleratorConfiguration> config_;
};

// TODO(jimmyxgong): Remove stub test after real implementation is available.
TEST_F(BrowserAcceleratorConfigurationTest, IsMutable) {
  ASSERT_FALSE(config_->IsMutable());
}

}  // namespace

}  // namespace ash

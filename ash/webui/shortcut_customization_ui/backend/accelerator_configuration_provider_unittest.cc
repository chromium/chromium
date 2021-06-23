// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public testing::Test {
 public:
  AcceleratorConfigurationProviderTest() = default;
  ~AcceleratorConfigurationProviderTest() override = default;
};

TEST_F(AcceleratorConfigurationProviderTest, DummyTest) {
  AcceleratorConfigurationProvider provider;
  EXPECT_TRUE(true);
}

}  // namespace shortcut_ui
}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public testing::Test {
 public:
  AcceleratorConfigurationProviderTest() {}
  ~AcceleratorConfigurationProviderTest() override {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  AcceleratorConfigurationProvider provider_;
};

TEST_F(AcceleratorConfigurationProviderTest, BrowserIsMutable) {
  base::RunLoop run_loop;
  // Verify that requesting IsMutable state for Browser accelerators returns
  // false.
  provider_.IsMutable(ash::mojom::AcceleratorSource::kBrowser,
                      base::BindLambdaForTesting([&](bool is_mutable) {
                        // Browser accelerators are not mutable.
                        EXPECT_FALSE(is_mutable);
                        run_loop.Quit();
                      }));
  run_loop.Run();
}

TEST_F(AcceleratorConfigurationProviderTest, AshIsMutable) {
  base::RunLoop run_loop;
  // Verify that requesting IsMutable state for Ash accelerators returns true.
  provider_.IsMutable(ash::mojom::AcceleratorSource::kAsh,
                      base::BindLambdaForTesting([&](bool is_mutable) {
                        // Ash accelerators are mutable.
                        EXPECT_TRUE(is_mutable);
                        run_loop.Quit();
                      }));
  run_loop.Run();
}

}  // namespace shortcut_ui

}  // namespace ash

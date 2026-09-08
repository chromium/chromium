// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"
#include "components/browser_actuator/public/features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class MockBrowserActuatorInternalsPage
    : public browser_actuator_internals::mojom::BrowserActuatorInternalsPage {
 public:
  MockBrowserActuatorInternalsPage() = default;
  ~MockBrowserActuatorInternalsPage() override = default;

  mojo::PendingRemote<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
  BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
      receiver_{this};
};

}  // namespace

class BrowserActuatorInternalsUIMojoImplTest : public testing::Test {
 public:
  BrowserActuatorInternalsUIMojoImplTest() = default;
  ~BrowserActuatorInternalsUIMojoImplTest() override = default;

  void SetUp() override {
    mojo_impl_ = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
        ui_remote_.BindNewPipeAndPassReceiver(),
        page_mock_.BindAndPassRemote());
  }

  void TearDown() override { mojo_impl_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockBrowserActuatorInternalsPage page_mock_;
  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      ui_remote_;
  std::unique_ptr<BrowserActuatorInternalsUIMojoImpl> mojo_impl_;
};

TEST_F(BrowserActuatorInternalsUIMojoImplTest, BindsMojoEndpoints) {
  EXPECT_TRUE(ui_remote_.is_bound());
  ui_remote_.FlushForTesting();
  EXPECT_TRUE(ui_remote_.is_connected());
}

TEST(BrowserActuatorInternalsUIConfigTest, IsWebUIEnabled) {
  base::test::ScopedFeatureList feature_list;
  BrowserActuatorInternalsUIConfig config;

  // Both features enabled -> WebUI is enabled.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator, kBrowserActuatorInternals},
      /*disabled_features=*/{});
  EXPECT_TRUE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Only kBrowserActuator enabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator},
      /*disabled_features=*/{kBrowserActuatorInternals});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Only kBrowserActuatorInternals enabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuatorInternals},
      /*disabled_features=*/{kBrowserActuator});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Both disabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kBrowserActuator, kBrowserActuatorInternals});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));
}

}  // namespace browser_actuator

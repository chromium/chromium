// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class FakeMultiDeviceSetupScreenView : public MultiDeviceSetupScreenView {
 public:
  FakeMultiDeviceSetupScreenView() = default;
  ~FakeMultiDeviceSetupScreenView() override = default;

  // MultiDeviceSetupScreenView:
  void Bind(MultiDeviceSetupScreen* screen) override {}
  void Show() override {}
  void Hide() override {}
};

}  // namespace

class MultiDeviceSetupScreenTest : public testing::Test {
 public:
  MultiDeviceSetupScreenTest() = default;
  ~MultiDeviceSetupScreenTest() override = default;

  // testing::Test:
  void SetUp() override {
    multi_device_setup_screen_ = std::make_unique<MultiDeviceSetupScreen>(
        &fake_multi_device_setup_screen_view_, base::DoNothing());
  }

  void TearDown() override {}

  std::unique_ptr<MultiDeviceSetupScreen> multi_device_setup_screen_;

  void VerifyUserChoicePaths() {
    histogram_tester_.ExpectTotalCount("MultiDeviceSetup.OOBE.UserChoice", 0);

    multi_device_setup_screen_->OnUserAction("setup-accepted");

    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kAccepted, 1);
    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kDeclined, 0);

    multi_device_setup_screen_->OnUserAction("setup-declined");

    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kAccepted, 1);
    histogram_tester_.ExpectBucketCount<
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice>(
        "MultiDeviceSetup.OOBE.UserChoice",
        MultiDeviceSetupScreen::MultiDeviceSetupOOBEUserChoice::kDeclined, 1);
  }

 private:
  base::HistogramTester histogram_tester_;

  // Accessory objects needed by MultiDeviceSetupScreen
  FakeMultiDeviceSetupScreenView fake_multi_device_setup_screen_view_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupScreenTest);
};

TEST_F(MultiDeviceSetupScreenTest, VerifyUserChoicePaths) {
  VerifyUserChoicePaths();
}

}  // namespace chromeos

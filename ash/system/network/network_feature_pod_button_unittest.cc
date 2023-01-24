// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_button.h"

#include <memory>

#include "ash/system/unified/feature_pod_button.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace {

class FakeNetworkFeaturePodButtonDelegate
    : public NetworkFeaturePodButton::Delegate {
 public:
  size_t feature_pod_button_theme_changed_count() {
    return feature_pod_button_theme_changed_count_;
  }

 private:
  // NetworkFeaturePodButton::Delegate:
  void OnFeaturePodButtonThemeChanged() override {
    ++feature_pod_button_theme_changed_count_;
  }

  size_t feature_pod_button_theme_changed_count_ = 0;
};

}  // namespace

class NetworkFeaturePodButtonTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    network_feature_pod_button_ =
        std::make_unique<NetworkFeaturePodButton>(/*controller=*/nullptr,
                                                  /*delegate=*/delegate());
  }

  void TearDown() override {
    network_feature_pod_button_.reset();

    AshTestBase::TearDown();
  }

  void CallOnThemeChanged() { network_feature_pod_button_->OnThemeChanged(); }

  FakeNetworkFeaturePodButtonDelegate* delegate() {
    return &fake_network_feature_pod_button_delegate_;
  }

 private:
  std::unique_ptr<NetworkFeaturePodButton> network_feature_pod_button_;
  FakeNetworkFeaturePodButtonDelegate fake_network_feature_pod_button_delegate_;
};

TEST_F(NetworkFeaturePodButtonTest, NotifiesDelegateWhenThemeChanges) {
  EXPECT_EQ(0u, delegate()->feature_pod_button_theme_changed_count());
  CallOnThemeChanged();
  EXPECT_EQ(1u, delegate()->feature_pod_button_theme_changed_count());
}

}  // namespace ash

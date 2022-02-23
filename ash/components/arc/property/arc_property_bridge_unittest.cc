// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/property/arc_property_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class FakePropertyInstance : public mojom::PropertyInstance {
 public:
  FakePropertyInstance() = default;
  ~FakePropertyInstance() override = default;

  FakePropertyInstance(const FakePropertyInstance&) = delete;
  FakePropertyInstance& operator=(const FakePropertyInstance&) = delete;

  void GetGcaMigrationProperty(
      GetGcaMigrationPropertyCallback callback) override {}

  void SetMinimizeOnBackButton(bool enable) override {
    minimize_on_back_ = enable;
  }

  absl::optional<bool> minimize_on_back() const { return minimize_on_back_; }

 private:
  absl::optional<bool> minimize_on_back_;
};

}  // namespace

class ArcPropertyBridgeTest : public testing::Test {
 public:
  ArcPropertyBridgeTest()
      : bridge_service_(std::make_unique<ArcBridgeService>()),
        property_bridge_(
            std::make_unique<ArcPropertyBridge>(nullptr,
                                                bridge_service_.get())) {}

  ~ArcPropertyBridgeTest() override { DestroyPropertyInstance(); }

  ArcPropertyBridgeTest(const ArcPropertyBridgeTest&) = delete;
  ArcPropertyBridgeTest& operator=(const ArcPropertyBridgeTest&) = delete;

  void SetUp() override {
    trial_list_resetter_ =
        std::make_unique<base::test::ScopedFieldTrialListResetter>();
    trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
  }

  void TearDown() override {
    trial_list_ = nullptr;
    trial_list_resetter_ = nullptr;
  }

  void CreatePropertyInstance() {
    instance_ = std::make_unique<FakePropertyInstance>();
    bridge_service_->property()->SetInstance(instance_.get());
    WaitForInstanceReady(bridge_service_->property());
  }

  void DestroyPropertyInstance() {
    if (!instance_)
      return;
    bridge_service_->property()->CloseInstance(instance_.get());
    instance_ = nullptr;
  }

  absl::optional<bool> GetMinimizeOnBackState() const {
    return instance_->minimize_on_back();
  }

 private:
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcPropertyBridge> property_bridge_;
  std::unique_ptr<FakePropertyInstance> instance_;

  std::unique_ptr<base::test::ScopedFieldTrialListResetter>
      trial_list_resetter_;
  std::unique_ptr<base::FieldTrialList> trial_list_;
};

TEST_F(ArcPropertyBridgeTest, MinimizeOnBackButtonDefault) {
  // The field trial is unset.
  CreatePropertyInstance();
  EXPECT_FALSE(GetMinimizeOnBackState().has_value());
}

TEST_F(ArcPropertyBridgeTest, MinimizeOnBackButtonEnabled) {
  base::FieldTrialList::CreateFieldTrial(
      ArcPropertyBridge::kMinimizeOnBackButtonTrialName,
      ArcPropertyBridge::kMinimizeOnBackButtonEnabled);
  CreatePropertyInstance();
  EXPECT_EQ(true, GetMinimizeOnBackState());
}

TEST_F(ArcPropertyBridgeTest, MinimizeOnBackButtonDisabled) {
  base::FieldTrialList::CreateFieldTrial(
      ArcPropertyBridge::kMinimizeOnBackButtonTrialName,
      ArcPropertyBridge::kMinimizeOnBackButtonDisabled);
  CreatePropertyInstance();
  EXPECT_EQ(false, GetMinimizeOnBackState());
}

}  // namespace arc

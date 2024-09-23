// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/accelerator_fetcher.h"

#include <memory>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_lookup.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace ash {

namespace {

const std::vector<AcceleratorAction>& actionIds = {
    AcceleratorAction::kSwitchToNextIme};

bool CompareAccelerators(
    const std::vector<AcceleratorLookup::AcceleratorDetails>& expected,
    const std::vector<mojom::StandardAcceleratorPropertiesPtr>& actual) {
  if (expected.size() != actual.size()) {
    return false;
  }

  for (size_t i = 0; i < expected.size(); ++i) {
    const bool accelerators_equal =
        expected[i].accelerator == actual[i]->accelerator;
    const bool key_display_equal =
        expected[i].key_display == actual[i]->key_display;
    if (!accelerators_equal || !key_display_equal) {
      return false;
    }
  }

  return true;
}

}  // namespace

class FakeAcceleratorFetcherObserver
    : public common::mojom::AcceleratorFetcherObserver {
 public:
  void OnAcceleratorsUpdated(
      AcceleratorAction _actionIds,
      std::vector<mojom::StandardAcceleratorPropertiesPtr> accelerators)
      override {
    accelerators_ = std::move(accelerators);
    ++accelerators_updated_called_count_;
  }

  int accelerators_updated_called_count() {
    return accelerators_updated_called_count_;
  }

  void set_accelerators_updated_called_count(int count) {
    accelerators_updated_called_count_ = count;
  }

  std::vector<mojom::StandardAcceleratorPropertiesPtr> accelerators_;
  int accelerators_updated_called_count_ = 0;
  mojo::Receiver<common::mojom::AcceleratorFetcherObserver> receiver{this};
};

class AcceleratorFetcherTest : public AshTestBase {
 public:
  AcceleratorFetcherTest() {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit,
                                           ::features::kShortcutCustomization},
                                          {});
  }

  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    accelerator_lookup_ = Shell::Get()->accelerator_lookup();
    accelerator_fetcher_ = std::make_unique<AcceleratorFetcher>();
    observer_ = std::make_unique<FakeAcceleratorFetcherObserver>();
    accelerator_fetcher_->ObserveAcceleratorChanges(
        actionIds, observer_->receiver.BindNewPipeAndPassRemote());
    accelerator_fetcher_->FlushMojoForTesting();
  }

  void TearDown() override {
    accelerator_fetcher_ = nullptr;
    accelerator_lookup_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AcceleratorFetcher> accelerator_fetcher_;
  raw_ptr<AcceleratorLookup> accelerator_lookup_;
  std::unique_ptr<FakeAcceleratorFetcherObserver> observer_;
};

TEST_F(AcceleratorFetcherTest, ObserveAcceleratorChanges) {
  observer_->set_accelerators_updated_called_count(0);
  const auto& expected_accelerators =
      accelerator_lookup_->GetAvailableAcceleratorsForAction(
          AcceleratorAction::kSwitchToNextIme);

  accelerator_fetcher_->OnAcceleratorsUpdated();
  accelerator_fetcher_->FlushMojoForTesting();
  EXPECT_EQ(observer_->accelerators_updated_called_count(),
            static_cast<int>(expected_accelerators.size()));
  EXPECT_EQ(observer_->accelerators_.size(), expected_accelerators.size());
  EXPECT_TRUE(
      CompareAccelerators(expected_accelerators, observer_->accelerators_));
}

}  // namespace ash

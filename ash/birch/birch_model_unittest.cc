// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

class TestModelObserver : public BirchModel::Observer {
 public:
  TestModelObserver() { Shell::Get()->birch_model()->AddObserver(this); }
  ~TestModelObserver() override {
    Shell::Get()->birch_model()->RemoveObserver(this);
  }

  void OnItemsChanged() override { item_changed_count_++; }

  int item_changed_count() { return item_changed_count_; }

 private:
  int item_changed_count_ = 0;
};

}  // namespace

class BirchModelTest : public AshTestBase {
 public:
  void SetUp() override {
    switches::SetIgnoreBirchSecretKeyForTest(true);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    switches::SetIgnoreBirchSecretKeyForTest(false);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kBirchFeature};
};

// Test that adding items to the model notifies observers.
TEST_F(BirchModelTest, AddItemNotifiesObservers) {
  BirchModel* model = Shell::Get()->birch_model();
  EXPECT_TRUE(model);

  TestModelObserver observer;
  EXPECT_EQ(observer.item_changed_count(), 0);

  model->AddItem(std::make_unique<BirchItem>("item 1"));
  model->AddItem(std::make_unique<BirchItem>("item 2"));

  // Adding items should notify that items have changed twice.
  EXPECT_EQ(observer.item_changed_count(), 2);
}

}  // namespace ash

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_application_menu_model.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kNumItemsEnabledHistogramName[] =
    "Ash.Shelf.Menu.NumItemsEnabledUponSelection";

const char kSelectedMenuItemIndexHistogramName[] =
    "Ash.Shelf.Menu.SelectedMenuItemIndex";

}  // namespace

// Test API to provide internal access to a ShelfApplicationMenuModel.
class ShelfApplicationMenuModelTestAPI {
 public:
  // Creates a test api to access the internals of the |menu|.
  explicit ShelfApplicationMenuModelTestAPI(ShelfApplicationMenuModel* menu)
      : menu_(menu) {}

  ShelfApplicationMenuModelTestAPI(const ShelfApplicationMenuModelTestAPI&) =
      delete;
  ShelfApplicationMenuModelTestAPI& operator=(
      const ShelfApplicationMenuModelTestAPI&) = delete;

  ~ShelfApplicationMenuModelTestAPI() = default;

  // Give public access to this metrics recording functions.
  void RecordMenuItemSelectedMetrics(int command_id,
                                     int num_menu_items_enabled) {
    menu_->RecordMenuItemSelectedMetrics(command_id, num_menu_items_enabled);
  }

 private:
  // The ShelfApplicationMenuModel to provide internal access to. Not owned.
  raw_ptr<ShelfApplicationMenuModel> menu_;
};

// Verifies the menu contents given an empty item list.
TEST(ShelfApplicationMenuModelTest, VerifyContentsWithNoMenuItems) {
  std::u16string title = u"title";
  ShelfApplicationMenuModel menu(title, {}, nullptr);
  // Expect the title and a separator.
  ASSERT_EQ(2u, menu.GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_TITLE, menu.GetTypeAt(0));
  EXPECT_EQ(title, menu.GetLabelAt(0));
  EXPECT_FALSE(menu.IsEnabledAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, menu.GetTypeAt(1));
}

// Verifies the menu contents given a non-empty item list.
TEST(ShelfApplicationMenuModelTest, VerifyContentsWithMenuItems) {
  ShelfApplicationMenuModel::Items items;
  std::u16string title1 = u"title1";
  std::u16string title2 = u"title2";
  std::u16string title3 = u"title3";
  items.push_back({static_cast<int>(items.size()), title1, gfx::ImageSkia()});
  items.push_back({static_cast<int>(items.size()), title2, gfx::ImageSkia()});
  items.push_back({static_cast<int>(items.size()), title3, gfx::ImageSkia()});

  std::u16string title = u"title";
  ShelfApplicationMenuModel menu(title, std::move(items), nullptr);
  ShelfApplicationMenuModelTestAPI menu_test_api(&menu);

  // Expect the title and the enabled items.
  ASSERT_EQ(5u, menu.GetItemCount());

  // The label title should not be enabled.
  EXPECT_EQ(ui::MenuModel::TYPE_TITLE, menu.GetTypeAt(0));
  EXPECT_EQ(title, menu.GetLabelAt(0));
  EXPECT_FALSE(menu.IsEnabledAt(0));

  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu.GetTypeAt(1));
  EXPECT_EQ(title1, menu.GetLabelAt(1));
  EXPECT_TRUE(menu.IsEnabledAt(1));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu.GetTypeAt(2));
  EXPECT_EQ(title2, menu.GetLabelAt(2));
  EXPECT_TRUE(menu.IsEnabledAt(2));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu.GetTypeAt(3));
  EXPECT_EQ(title3, menu.GetLabelAt(3));
  EXPECT_TRUE(menu.IsEnabledAt(3));
}

// Verifies RecordMenuItemSelectedMetrics uses the correct histogram buckets.
TEST(ShelfApplicationMenuModelTest, VerifyHistogramBuckets) {
  const int kCommandId = 3;
  const int kNumMenuItemsEnabled = 7;

  base::HistogramTester histogram_tester;
  ShelfApplicationMenuModel menu(u"title", {}, nullptr);
  ShelfApplicationMenuModelTestAPI menu_test_api(&menu);
  menu_test_api.RecordMenuItemSelectedMetrics(kCommandId, kNumMenuItemsEnabled);

  histogram_tester.ExpectTotalCount(kNumItemsEnabledHistogramName, 1);
  histogram_tester.ExpectBucketCount(kNumItemsEnabledHistogramName,
                                     kNumMenuItemsEnabled, 1);

  histogram_tester.ExpectTotalCount(kSelectedMenuItemIndexHistogramName, 1);
  histogram_tester.ExpectBucketCount(kSelectedMenuItemIndexHistogramName,
                                     kCommandId, 1);
}

// Verify histogram data is recorded when ExecuteCommand is called.
TEST(ShelfApplicationMenuModelTest, VerifyHistogramOnExecute) {
  base::HistogramTester histogram_tester;

  ShelfApplicationMenuModel::Items items(1);
  std::u16string title = u"title";
  ShelfApplicationMenuModel menu(title, std::move(items), nullptr);
  menu.ExecuteCommand(0, 0);

  histogram_tester.ExpectTotalCount(kNumItemsEnabledHistogramName, 1);
  histogram_tester.ExpectTotalCount(kSelectedMenuItemIndexHistogramName, 1);
}

}  // namespace ash

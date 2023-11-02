// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item.h"

#include "ash/app_list/model/app_list_item_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(AppListItemTest, SetIsNewInstall) {
  AppListItem item("id");

  class Observer : public AppListItemObserver {
   public:
    // AppListItemObserver:
    void ItemIsNewInstallChanged() override { is_new_install_changed_++; }

    int is_new_install_changed_ = 0;
  } observer;

  // Adding an observer does not notify it.
  item.AddObserver(&observer);
  EXPECT_EQ(observer.is_new_install_changed_, 0);

  // Setting new install notifies the observer.
  item.SetIsNewInstall(true);
  EXPECT_TRUE(item.is_new_install());
  EXPECT_EQ(observer.is_new_install_changed_, 1);

  // Clearing new install notifies the observer.
  item.SetIsNewInstall(false);
  EXPECT_FALSE(item.is_new_install());
  EXPECT_EQ(observer.is_new_install_changed_, 2);

  item.RemoveObserver(&observer);
}

}  // namespace ash

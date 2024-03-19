// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item_remover.h"

#include <memory>

#include "ash/birch/birch_item.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class BirchItemRemoverTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

    item_remover_ = std::make_unique<BirchItemRemover>(test_dir_.GetPath(),
                                                       run_loop_.QuitClosure());

    EXPECT_FALSE(item_remover_->Initialized());
    run_loop_.Run();
    EXPECT_TRUE(item_remover_->Initialized());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::ScopedTempDir test_dir_;
  std::unique_ptr<BirchItemRemover> item_remover_;
};

TEST_F(BirchItemRemoverTest, RemoveTab) {
  BirchTabItem item0(u"item0", GURL("https://example.com/0"), base::Time(),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);
  BirchTabItem item1(u"item1", GURL("https://example.com/1"), base::Time(),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);
  BirchTabItem item2(u"item2", GURL("https://example.com/2"), base::Time(),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);
  BirchTabItem item3(u"item3", GURL("https://example.com/3"), base::Time(),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);
  std::vector<BirchTabItem> tab_items = {item0, item1, item2, item3};

  // Filter `tab_items` before any items are removed. The list should remain
  // unchanged.
  item_remover_->FilterRemovedTabs(&tab_items);
  ASSERT_EQ(4u, tab_items.size());

  // Remove `item2`, and filter it from the list of tabs.
  item_remover_->RemoveItem(&item2);
  item_remover_->FilterRemovedTabs(&tab_items);

  // Check that `item2` is filtered out.
  ASSERT_EQ(3u, tab_items.size());
  EXPECT_EQ(tab_items, std::vector({item0, item1, item3}));
}

}  // namespace
}  // namespace ash

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

TEST_F(BirchItemRemoverTest, RemoveSelfShareItems) {
  BirchSelfShareItem item0(u"item0_guid", u"item0_title",
                           GURL("https://example.com/0"), base::Time(),
                           u"device_name", SecondaryIconType::kTabFromDesktop,
                           base::DoNothing());
  BirchSelfShareItem item1(u"item1_guid", u"item1_title",
                           GURL("https://example.com/1"), base::Time(),
                           u"device_name", SecondaryIconType::kTabFromDesktop,
                           base::DoNothing());
  BirchSelfShareItem item2(u"item2_guid", u"item2_title",
                           GURL("https://example.com/2"), base::Time(),
                           u"device_name", SecondaryIconType::kTabFromDesktop,
                           base::DoNothing());
  BirchSelfShareItem item3(u"item3_guid", u"item3_title",
                           GURL("https://example.com/3"), base::Time(),
                           u"device_name", SecondaryIconType::kTabFromDesktop,
                           base::DoNothing());
  std::vector<BirchSelfShareItem> self_share_items = {item0, item1, item2,
                                                      item3};

  // Filter `self_share_items` before any items are removed. The list should
  // remain unchanged.
  item_remover_->FilterRemovedSelfShareItems(&self_share_items);
  ASSERT_EQ(4u, self_share_items.size());

  // Remove `item2`, and filter it from the list of tabs.
  item_remover_->RemoveItem(&item2);
  item_remover_->FilterRemovedSelfShareItems(&self_share_items);

  // Check that `item2` is filtered out.
  ASSERT_EQ(3u, self_share_items.size());
  EXPECT_EQ(self_share_items, std::vector({item0, item1, item3}));
}

TEST_F(BirchItemRemoverTest, RemoveCalendarItem) {
  BirchCalendarItem item0(u"Event 0", /*start_time=*/base::Time(),
                          /*end_time=*/base::Time(), /*calendar_url=*/GURL(),
                          /*conference_url=*/GURL(), /*event_id=*/"000",
                          /*all_day_event=*/false);
  BirchCalendarItem item1(u"Event 1", /*start_time=*/base::Time(),
                          /*end_time=*/base::Time(), /*calendar_url=*/GURL(),
                          /*conference_url=*/GURL(), /*event_id=*/"111",
                          /*all_day_event=*/false);
  BirchCalendarItem item2(u"Event 2", /*start_time=*/base::Time(),
                          /*end_time=*/base::Time(), /*calendar_url=*/GURL(),
                          /*conference_url=*/GURL(), /*event_id=*/"222",
                          /*all_day_event=*/false);
  std::vector<BirchCalendarItem> calendar_items = {item0, item1, item2};

  // Filter `calendar_items` before any items are removed. The list should
  // remain unchanged.
  item_remover_->FilterRemovedCalendarItems(&calendar_items);
  ASSERT_EQ(3u, calendar_items.size());

  // Remove `item1`, and filter it from the list of calendar items.
  item_remover_->RemoveItem(&item1);
  item_remover_->FilterRemovedCalendarItems(&calendar_items);

  // Check that `item1` is filtered out.
  ASSERT_EQ(2u, calendar_items.size());
  EXPECT_EQ(calendar_items, std::vector({item0, item2}));
}

TEST_F(BirchItemRemoverTest, RemoveFileItem) {
  BirchFileItem item0(base::FilePath(), "title", u"justification", base::Time(),
                      "file_id_0", "icon_url");
  BirchFileItem item1(base::FilePath(), "title", u"justification", base::Time(),
                      "file_id_1", "icon_url");
  BirchFileItem item2(base::FilePath(), "title", u"justification", base::Time(),
                      "file_id_2", "icon_url");
  std::vector<BirchFileItem> file_items = {item0, item1, item2};

  // Filter `file_items` before any items are removed. The list should remain
  // unchanged.
  item_remover_->FilterRemovedFileItems(&file_items);
  ASSERT_EQ(3u, file_items.size());

  // Remove `item1`, and filter it from the list of file items.
  item_remover_->RemoveItem(&item1);
  item_remover_->FilterRemovedFileItems(&file_items);

  // Check that `item1` is filtered out.
  ASSERT_EQ(2u, file_items.size());
  EXPECT_EQ(file_items, std::vector({item0, item2}));
}

TEST_F(BirchItemRemoverTest, RemoveAttachmentItem) {
  BirchAttachmentItem item0(u"attachment 0",
                            /*file_url=*/GURL(),
                            /*icon_url=*/GURL(),
                            /*start_time=*/base::Time(),
                            /*end_time=*/base::Time(),
                            /*file_id=*/"file_id_0");
  BirchAttachmentItem item1(u"attachment 1",
                            /*file_url=*/GURL(),
                            /*icon_url=*/GURL(),
                            /*start_time=*/base::Time(),
                            /*end_time=*/base::Time(),
                            /*file_id=*/"file_id_1");
  BirchAttachmentItem item2(u"attachment 2",
                            /*file_url=*/GURL(),
                            /*icon_url=*/GURL(),
                            /*start_time=*/base::Time(),
                            /*end_time=*/base::Time(),
                            /*file_id=*/"file_id_2");
  std::vector<BirchAttachmentItem> attachment_items = {item0, item1, item2};

  // Filter `attachment_items` before any items are removed. The list should
  // remain unchanged.
  item_remover_->FilterRemovedAttachmentItems(&attachment_items);
  ASSERT_EQ(3u, attachment_items.size());

  // Remove `item1`, and filter it from the list of attachment items.
  item_remover_->RemoveItem(&item1);
  item_remover_->FilterRemovedAttachmentItems(&attachment_items);

  // Check that `item1` is filtered out.
  ASSERT_EQ(2u, attachment_items.size());
  EXPECT_EQ(attachment_items, std::vector({item0, item2}));
}

}  // namespace
}  // namespace ash

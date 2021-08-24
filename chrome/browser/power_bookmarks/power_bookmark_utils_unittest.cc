// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/power_bookmarks/power_bookmark_utils.h"
#include "chrome/browser/power_bookmarks/proto/power_bookmark_meta.pb.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {
namespace {

const std::string kLeadImageUrl = "image.png";

const char16_t kExampleTitle[] = u"Title";
const std::string kExampleUrl = "https://example.com";

class PowerBookmarkUtilsTest : public testing::Test {
 protected:
  bookmarks::BookmarkModel* CreateTestModel() {
    std::unique_ptr<bookmarks::BookmarkModel> model(
        bookmarks::TestBookmarkClient::CreateModel());
    const bookmarks::BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kExampleTitle, GURL(kExampleUrl));
    return model.release();
  }
};

TEST_F(PowerBookmarkUtilsTest, TestAddAndAccess) {
  bookmarks::BookmarkModel* model = CreateTestModel();
  const bookmarks::BookmarkNode* node =
      model->bookmark_bar_node()->children().front().get();

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  meta->mutable_lead_image()->set_url(kLeadImageUrl);

  SetNodePowerBookmarkMeta(model, node, std::move(meta));

  const std::unique_ptr<PowerBookmarkMeta> fetched_meta =
      GetNodePowerBookmarkMeta(node);

  EXPECT_EQ(kLeadImageUrl, fetched_meta->lead_image().url());
}

TEST_F(PowerBookmarkUtilsTest, TestAddAndDelete) {
  bookmarks::BookmarkModel* model = CreateTestModel();
  const bookmarks::BookmarkNode* node =
      model->bookmark_bar_node()->children().front().get();

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  meta->mutable_lead_image()->set_url(kLeadImageUrl);

  SetNodePowerBookmarkMeta(model, node, std::move(meta));

  DeleteNodePowerBookmarkMeta(model, node);

  const std::unique_ptr<PowerBookmarkMeta> fetched_meta =
      GetNodePowerBookmarkMeta(node);

  EXPECT_EQ(nullptr, fetched_meta.get());
}

}  // namespace
}  // namespace power_bookmarks

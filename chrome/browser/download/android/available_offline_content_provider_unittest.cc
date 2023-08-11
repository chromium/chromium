// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/available_offline_content_provider.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/available_offline_content.mojom-test-utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace android {
namespace {

using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemState;
using offline_items_collection::OfflineItemVisuals;
using testing::_;
const char kProviderNamespace[] = "offline_pages";

OfflineItem UninterestingImageItem() {
  OfflineItem item;
  item.original_url = GURL("https://uninteresting");
  item.filter = offline_items_collection::FILTER_IMAGE;
  item.id.id = "UninterestingItem";
  item.id.name_space = kProviderNamespace;
  return item;
}

OfflineItem OfflinePageItem() {
  OfflineItem item;
  item.original_url = GURL("https://already_read");
  item.filter = offline_items_collection::FILTER_PAGE;
  item.id.id = "NonSuggestedOfflinePage";
  item.id.name_space = kProviderNamespace;
  item.last_accessed_time = base::Time::Now();
  return item;
}

OfflineItem SuggestedOfflinePageItem() {
  OfflineItem item;
  item.original_url = GURL("https://read_prefetched_page");
  item.filter = offline_items_collection::FILTER_PAGE;
  item.id.id = "SuggestedOfflinePage";
  item.id.name_space = kProviderNamespace;
  item.is_suggested = true;
  item.title = "Page Title";
  item.description = "snippet";
  // Using Time::Now() isn't ideal, but this should result in "4 hours ago"
  // even if the test takes 1 hour to run.
  item.creation_time = base::Time::Now() - base::Minutes(60 * 3.5);
  item.last_accessed_time = base::Time::Now();
  item.attribution = "attribution";
  return item;
}

OfflineItem VideoItem() {
  OfflineItem item;
  item.original_url = GURL("https://video");
  item.filter = offline_items_collection::FILTER_VIDEO;
  item.id.id = "VideoItem";
  item.id.name_space = kProviderNamespace;
  return item;
}

OfflineItem AudioItem() {
  OfflineItem item;
  item.original_url = GURL("https://audio");
  item.filter = offline_items_collection::FILTER_AUDIO;
  item.id.id = "AudioItem";
  item.id.name_space = kProviderNamespace;
  return item;
}

OfflineItem TransientItem() {
  OfflineItem item = VideoItem();
  item.is_transient = true;
  return item;
}

OfflineItem OffTheRecordItem() {
  OfflineItem item = VideoItem();
  item.is_off_the_record = true;
  return item;
}

OfflineItem IncompleteItem() {
  OfflineItem item = VideoItem();
  item.state = OfflineItemState::PAUSED;
  return item;
}

OfflineItem DangerousItem() {
  OfflineItem item = VideoItem();
  item.is_dangerous = true;
  return item;
}

OfflineItemVisuals TestThumbnail() {
  OfflineItemVisuals visuals;
  visuals.icon = gfx::test::CreateImage(2, 4);
  visuals.custom_favicon = gfx::test::CreateImage(4, 4);
  return visuals;
}

class AvailableOfflineContentTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content_provider_ = std::make_unique<
        offline_items_collection::MockOfflineContentProvider>();
    provider_ = std::make_unique<AvailableOfflineContentProvider>(
        main_rfh()->GetProcess()->GetID());

    aggregator_ =
        OfflineContentAggregatorFactory::GetForKey(profile()->GetProfileKey());
    aggregator_->RegisterProvider(kProviderNamespace, content_provider_.get());
    content_provider_->SetVisuals({});
  }

  void TearDown() override {
    provider_.release();
    content_provider_.release();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::tuple<bool, std::vector<chrome::mojom::AvailableOfflineContentPtr>>
  ListAndWait() {
    base::test::TestFuture<
        bool, std::vector<chrome::mojom::AvailableOfflineContentPtr>>
        future;
    provider_->List(future.GetCallback());
    return future.Take();
  }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_ =
      std::make_unique<base::test::ScopedFeatureList>();
  raw_ptr<OfflineContentAggregator> aggregator_;
  std::unique_ptr<offline_items_collection::MockOfflineContentProvider>
      content_provider_;
  std::unique_ptr<AvailableOfflineContentProvider> provider_;
};

TEST_F(AvailableOfflineContentTest, NoContent) {
  auto [list_visible_by_prefs, suggestions] = ListAndWait();

  EXPECT_TRUE(suggestions.empty());
  EXPECT_TRUE(list_visible_by_prefs);
}

TEST_F(AvailableOfflineContentTest, TooFewInterestingItems) {
  // Adds items so that we're one-ff of reaching the minimum required count so
  // that any extra item considered interesting would effect the results.
  content_provider_->SetItems({UninterestingImageItem(), OfflinePageItem(),
                               SuggestedOfflinePageItem(), VideoItem(),
                               TransientItem(), OffTheRecordItem(),
                               IncompleteItem(), DangerousItem()});

  // Call List().
  auto [list_visible_by_prefs, suggestions] = ListAndWait();

  // As interesting items are below the minimum to show, nothing should be
  // reported.
  EXPECT_TRUE(suggestions.empty());
  EXPECT_TRUE(list_visible_by_prefs);
}

TEST_F(AvailableOfflineContentTest, FourInterestingItems) {
  // We need at least 4 interesting items for anything to show up at all.
  content_provider_->SetItems({UninterestingImageItem(), VideoItem(),
                               SuggestedOfflinePageItem(), AudioItem(),
                               OfflinePageItem()});

  content_provider_->SetVisuals(
      {{SuggestedOfflinePageItem().id, TestThumbnail()}});

  // Call List().
  auto [list_visible_by_prefs, suggestions] = ListAndWait();

  // Check that the right suggestions have been received in order.
  EXPECT_EQ(3ul, suggestions.size());
  EXPECT_EQ(SuggestedOfflinePageItem().id.id, suggestions[0]->id);
  EXPECT_EQ(VideoItem().id.id, suggestions[1]->id);
  EXPECT_EQ(AudioItem().id.id, suggestions[2]->id);
  EXPECT_TRUE(list_visible_by_prefs);

  // For a single suggestion, make sure all the fields are correct. We can
  // assume the other items match.
  const chrome::mojom::AvailableOfflineContentPtr& first = suggestions[0];
  const OfflineItem page_item = SuggestedOfflinePageItem();
  EXPECT_EQ(page_item.id.id, first->id);
  EXPECT_EQ(page_item.id.name_space, first->name_space);
  EXPECT_EQ(page_item.title, first->title);
  EXPECT_EQ(page_item.description, first->snippet);
  EXPECT_EQ("4 hours ago", first->date_modified);
  // At the time of writing this test, the output was:
  // data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAAECAYAAACk7+45AAAAFk
  // lEQVQYlWNk+M/wn4GBgYGJAQowGQBCcgIG00vTRwAAAABJRU5ErkJggg==
  // Since other encodings are possible, just check the prefix. PNGs all have
  // the same 8 byte header.
  EXPECT_TRUE(base::StartsWith(first->thumbnail_data_uri.spec(),
                               "data:image/png;base64,iVBORw0K",
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::StartsWith(first->favicon_data_uri.spec(),
                               "data:image/png;base64,iVBORw0K",
                               base::CompareCase::SENSITIVE));
  EXPECT_EQ(page_item.attribution, first->attribution);
}

TEST_F(AvailableOfflineContentTest, ListVisibilityChanges) {
  // We need at least 4 interesting items for anything to show up at all.
  content_provider_->SetItems({UninterestingImageItem(), VideoItem(),
                               SuggestedOfflinePageItem(), AudioItem(),
                               OfflinePageItem()});

  content_provider_->SetVisuals(
      {{SuggestedOfflinePageItem().id, TestThumbnail()}});
  // Set pref to hide the list.
  profile()->GetPrefs()->SetBoolean(feed::prefs::kArticlesListVisible, false);

  // Call List().
  auto [list_visible_by_prefs, suggestions] = ListAndWait();

  // Check that suggestions have been received and the list is not visible.
  EXPECT_EQ(3ul, suggestions.size());
  EXPECT_FALSE(list_visible_by_prefs);

  // Simulate visibility changed by the user to "shown".
  provider_->ListVisibilityChanged(true);

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(feed::prefs::kArticlesListVisible));

  // Call List() again and check list is not visible.
  std::tie(list_visible_by_prefs, suggestions) = ListAndWait();
  EXPECT_TRUE(list_visible_by_prefs);
}

}  // namespace
}  // namespace android

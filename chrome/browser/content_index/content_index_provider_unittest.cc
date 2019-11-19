// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "content/public/browser/content_index_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemVisuals;
using offline_items_collection::UpdateDelta;
using testing::_;

constexpr int64_t kServiceWorkerRegistrationId = 42;
constexpr double kEngagementScore = 42.0;
const GURL kLaunchURL = GURL("https://example.com/foo");
const url::Origin kOrigin = url::Origin::Create(kLaunchURL.GetOrigin());

class ContentIndexProviderImplTest : public testing::Test,
                                     public OfflineContentProvider::Observer {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_.CreateHistoryService(/* delete_file= */ true,
                                              /* no_db= */ false));

    auto* service = SiteEngagementService::Get(&profile_);
    service->ResetBaseScoreForURL(kOrigin.GetURL(), kEngagementScore);
    provider_ = std::make_unique<ContentIndexProviderImpl>(&profile_);
    provider_->AddObserver(this);
  }

  void TearDown() override { provider_->RemoveObserver(this); }

  // OfflineContentProvider::Observer implementation.
  MOCK_METHOD1(OnItemsAdded,
               void(const OfflineContentProvider::OfflineItemList& items));
  MOCK_METHOD1(OnItemRemoved, void(const ContentId& id));
  void OnItemUpdated(const OfflineItem& item,
                     const base::Optional<UpdateDelta>& update_delta) override {
    NOTREACHED();
  }

  content::ContentIndexEntry CreateEntry(const std::string& id) {
    auto description = blink::mojom::ContentDescription::New(
        id, "title", "description", blink::mojom::ContentCategory::ARTICLE,
        std::vector<blink::mojom::ContentIconDefinitionPtr>(), "launch_url");
    return content::ContentIndexEntry(kServiceWorkerRegistrationId,
                                      std::move(description), kLaunchURL,
                                      base::Time::Now());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ContentIndexProviderImpl> provider_;
};

TEST_F(ContentIndexProviderImplTest, OfflineItemCreation) {
  std::vector<OfflineItem> items;
  {
    EXPECT_CALL(*this, OnItemsAdded(_)).WillOnce(testing::SaveArg<0>(&items));
    provider_->OnContentAdded(CreateEntry("id"));
  }
  ASSERT_EQ(items.size(), 1u);
  const auto& item = items[0];

  EXPECT_FALSE(item.id.name_space.empty());
  EXPECT_FALSE(item.id.id.empty());
  EXPECT_FALSE(item.title.empty());
  EXPECT_FALSE(item.description.empty());
  EXPECT_FALSE(item.is_transient);
  EXPECT_TRUE(item.is_suggested);
  EXPECT_TRUE(item.is_openable);
  EXPECT_EQ(item.page_url, kLaunchURL);
  EXPECT_EQ(item.content_quality_score, kEngagementScore / 100.0);
}

TEST_F(ContentIndexProviderImplTest, ObserverUpdates) {
  {
    EXPECT_CALL(*this, OnItemsAdded(_));
    provider_->OnContentAdded(CreateEntry("id"));
  }

  // Adding an already existing ID should call update.
  {
    EXPECT_CALL(*this, OnItemRemoved(_)).Times(1);
    EXPECT_CALL(*this, OnItemsAdded(_)).Times(1);
    provider_->OnContentAdded(CreateEntry("id"));
  }

  {
    EXPECT_CALL(*this, OnItemRemoved(_));
    provider_->OnContentDeleted(kServiceWorkerRegistrationId, kOrigin, "id");
  }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_page_cache.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicMediaPageCacheTest : public testing::Test {};

TEST_F(GlicMediaPageCacheTest, AddsInOrder) {
  GlicMediaPageCache cache;
  GlicMediaPageCache::Entry entry1;
  GlicMediaPageCache::Entry entry2;

  // First item goes at the front(!).
  cache.PlaceAtFront(&entry1);
  EXPECT_EQ(cache.front(), &entry1);

  // Second item should push it back.
  cache.PlaceAtFront(&entry2);
  EXPECT_EQ(cache.front(), &entry2);
  EXPECT_EQ(cache.front()->next(), &entry1);

  // Re-adding the first item should work, and move it to the front again.
  cache.PlaceAtFront(&entry1);
  EXPECT_EQ(cache.front(), &entry1);
  EXPECT_EQ(cache.front()->next(), &entry2);
}

TEST_F(GlicMediaPageCacheTest, CacheClearsOnDestruction) {
  std::unique_ptr<GlicMediaPageCache> cache =
      std::make_unique<GlicMediaPageCache>();
  GlicMediaPageCache::Entry entry;
  EXPECT_EQ(entry.cache(), nullptr);

  cache->PlaceAtFront(&entry);
  EXPECT_NE(entry.next(), nullptr);
  EXPECT_EQ(entry.cache(), cache.get());

  // Reset the cache and make sure that `entry` was removed.
  cache.reset();
  EXPECT_EQ(entry.next(), nullptr);
  EXPECT_EQ(entry.cache(), nullptr);
}

TEST_F(GlicMediaPageCacheTest, EntriesAutoRemoveOnDestruction) {
  GlicMediaPageCache cache;

  std::unique_ptr<GlicMediaPageCache::Entry> entry =
      std::make_unique<GlicMediaPageCache::Entry>();
  cache.PlaceAtFront(entry.get());
  EXPECT_FALSE(cache.cache_for_testing().empty());

  entry.reset();
  EXPECT_TRUE(cache.cache_for_testing().empty());
}

}  // namespace glic

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"

#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Pair;

TEST(FileSystemProviderContentLRUCacheTest, InitialOrderIsRespectOnInit) {
  ContentLRUCache lru_cache;
  std::list<PathContextPair> list;
  list.emplace_back(base::FilePath("/a.txt"), CacheFileContext("versionA"));
  list.emplace_back(base::FilePath("/b.txt"), CacheFileContext("versionA"));
  lru_cache.Init(std::move(list));

  // Ensure the LRU cache is initialized in the order that was supplied.
  EXPECT_THAT(lru_cache, ElementsAre(Pair(base::FilePath("/a.txt"), _),
                                     Pair(base::FilePath("/b.txt"), _)));

  // Get a value from the cache.
  lru_cache.Get(base::FilePath("/b.txt"));

  // Expect that "b.txt" has been bumped up the recency list.
  EXPECT_THAT(lru_cache, ElementsAre(Pair(base::FilePath("/b.txt"), _),
                                     Pair(base::FilePath("/a.txt"), _)));
}

}  // namespace
}  // namespace ash::file_system_provider

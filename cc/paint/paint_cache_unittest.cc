// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_cache.h"

#include "base/hash/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr size_t kDefaultBudget = 1024u;

SkPath CreatePath() {
  SkPath path;
  path.addCircle(2, 2, 5);
  return path;
}

sk_sp<SkRuntimeEffect> GetEffect() {
  constexpr static char kShaderString[] =
      "vec4 main(vec2 uv) {return vec4(0.);}";
  auto result = SkRuntimeEffect::MakeForShader(SkString(kShaderString));
  CHECK(result.effect);
  return result.effect;
}

class PaintCacheTest : public ::testing::TestWithParam<uint32_t> {
 public:
  PaintCacheDataType GetType() {
    return static_cast<PaintCacheDataType>(GetParam());
  }
};

TEST_P(PaintCacheTest, ClientBasic) {
  ClientPaintCache client_cache(kDefaultBudget);
  EXPECT_FALSE(client_cache.Get(GetType(), 1u));
  client_cache.Put(GetType(), 1u, 1u);
  EXPECT_TRUE(client_cache.Get(GetType(), 1u));
}

TEST_P(PaintCacheTest, ClientPurgeForBudgeting) {
  ClientPaintCache client_cache(kDefaultBudget);
  client_cache.Put(GetType(), 1u, kDefaultBudget - 100);
  client_cache.Put(GetType(), 2u, kDefaultBudget);
  client_cache.Put(GetType(), 3u, kDefaultBudget);
  EXPECT_EQ(client_cache.bytes_used(), 3 * kDefaultBudget - 100);
  client_cache.FinalizePendingEntries();

  ClientPaintCache::PurgedData purged_data;
  client_cache.Purge(&purged_data);
  EXPECT_EQ(client_cache.bytes_used(), kDefaultBudget);
  const auto& ids = purged_data[static_cast<uint32_t>(GetType())];
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], 1u);
  EXPECT_EQ(ids[1], 2u);

  EXPECT_FALSE(client_cache.Get(GetType(), 1u));
  EXPECT_FALSE(client_cache.Get(GetType(), 2u));
  EXPECT_TRUE(client_cache.Get(GetType(), 3u));
}

TEST_P(PaintCacheTest, ClientPurgeAll) {
  ClientPaintCache client_cache(kDefaultBudget);
  client_cache.Put(GetType(), 1u, 1u);
  EXPECT_EQ(client_cache.bytes_used(), 1u);
  client_cache.FinalizePendingEntries();

  EXPECT_TRUE(client_cache.PurgeAll());
  EXPECT_EQ(client_cache.bytes_used(), 0u);
  EXPECT_FALSE(client_cache.PurgeAll());
}

TEST_P(PaintCacheTest, CommitPendingEntries) {
  ClientPaintCache client_cache(kDefaultBudget);

  client_cache.Put(GetType(), 1u, 1u);
  EXPECT_TRUE(client_cache.Get(GetType(), 1u));
  client_cache.AbortPendingEntries();
  EXPECT_FALSE(client_cache.Get(GetType(), 1u));

  client_cache.Put(GetType(), 1u, 1u);
  client_cache.FinalizePendingEntries();
  EXPECT_TRUE(client_cache.Get(GetType(), 1u));
}

TEST_P(PaintCacheTest, ServiceBasic) {
  ServicePaintCache service_cache;
  switch (GetType()) {
    case PaintCacheDataType::kPath: {
      auto path = CreatePath();
      auto id = path.getGenerationID();
      SkPath cached_path;
      EXPECT_EQ(false, service_cache.GetPath(id, &cached_path));
      service_cache.PutPath(id, path);
      EXPECT_EQ(true, service_cache.GetPath(id, &cached_path));
      EXPECT_EQ(path, cached_path);
      service_cache.Purge(GetType(), 1, &id);
      EXPECT_EQ(false, service_cache.GetPath(id, &cached_path));

      service_cache.PutPath(id, path);
    } break;
    case PaintCacheDataType::kSkRuntimeEffect: {
      auto effect = GetEffect();
      auto id = base::PersistentHash(effect->source());
      sk_sp<SkRuntimeEffect> cached_effect = nullptr;
      EXPECT_FALSE(service_cache.GetEffect(id, &cached_effect));
      service_cache.PutEffect(id, effect);
      EXPECT_TRUE(service_cache.GetEffect(id, &cached_effect));
      EXPECT_EQ(effect, cached_effect);
      service_cache.Purge(GetType(), 1u, &id);
      EXPECT_FALSE(service_cache.GetEffect(id, &cached_effect));

      service_cache.PutEffect(id, effect);
      break;
    }
  }

  EXPECT_FALSE(service_cache.IsEmpty());
  service_cache.PurgeAll();
  EXPECT_TRUE(service_cache.IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    PaintCacheTest,
    ::testing::Values(
        static_cast<uint32_t>(PaintCacheDataType::kPath),
        static_cast<uint32_t>(PaintCacheDataType::kSkRuntimeEffect)));

}  // namespace
}  // namespace cc

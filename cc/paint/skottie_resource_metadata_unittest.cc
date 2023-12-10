// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_resource_metadata.h"

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(SkottieResourceMetadataTest, SkottieResourceMetadataMapRegistersAssets) {
  SkottieResourceMetadataMap resource_map;
  ASSERT_TRUE(
      resource_map.RegisterAsset("test-resource-path-1", "test-resource-name-1",
                                 "test-resource-id-1", gfx::Size(100, 100)));
  ASSERT_TRUE(resource_map.RegisterAsset("test-resource-path-2",
                                         "test-resource-name-2",
                                         "test-resource-id-2", std::nullopt));
  EXPECT_THAT(
      resource_map.asset_storage(),
      UnorderedElementsAre(
          Pair("test-resource-id-1",
               FieldsAre(base::FilePath(
                             FILE_PATH_LITERAL(
                                 "test-resource-path-1/test-resource-name-1"))
                             .NormalizePathSeparators(),
                         Optional(gfx::Size(100, 100)))),
          Pair("test-resource-id-2",
               FieldsAre(base::FilePath(
                             FILE_PATH_LITERAL(
                                 "test-resource-path-2/test-resource-name-2"))
                             .NormalizePathSeparators(),
                         Eq(std::nullopt)))));
}

TEST(SkottieResourceMetadataTest,
     SkottieResourceMetadataMapRejectsDuplicateAssets) {
  SkottieResourceMetadataMap resource_map;
  ASSERT_TRUE(resource_map.RegisterAsset("test-resource-path-1",
                                         "test-resource-name-1",
                                         "test-resource-id-1", std::nullopt));
  EXPECT_FALSE(resource_map.RegisterAsset("test-resource-path-2",
                                          "test-resource-name-2",
                                          "test-resource-id-1", std::nullopt));
}

TEST(SkottieResourceMetadataTest,
     SkottieResourceMetadataMapRejectsEmptyAssets) {
  SkottieResourceMetadataMap resource_map;
  EXPECT_FALSE(resource_map.RegisterAsset("test-resource-path",
                                          "test-resource-name",
                                          /*resource_id=*/"", std::nullopt));
}

TEST(SkottieResourceMetadataTest,
     SkottieResourceMetadataMapRejectsAbsoluteResourceNames) {
  SkottieResourceMetadataMap resource_map;
  EXPECT_FALSE(resource_map.RegisterAsset("test-resource-path",
                                          "/absolute-resource-name",
                                          /*resource_id=*/"", std::nullopt));
}

TEST(SkottieResourceMetadataTest, HashSkottieResourceIdReturnsMatchingHashes) {
  EXPECT_THAT(HashSkottieResourceId("test-resource-id-1"),
              Eq(HashSkottieResourceId("test-resource-id-1")));
}

TEST(SkottieResourceMetadataTest, HashSkottieResourceIdReturnsDifferentHashes) {
  EXPECT_THAT(HashSkottieResourceId("test-resource-id-1"),
              Ne(HashSkottieResourceId("test-resource-id-2")));
}

}  // namespace
}  // namespace cc

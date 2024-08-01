// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/skottie_wrapper.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_mru_resource_provider.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::FloatNear;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Key;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr float kMarkerEpsilon = .01f;

class MockFrameDataCallback {
 public:
  MOCK_METHOD(SkottieWrapper::FrameDataFetchResult,
              OnAssetLoaded,
              (SkottieResourceIdHash asset_id_hash,
               float t,
               sk_sp<SkImage>& image_out,
               SkSamplingOptions& sampling_out));

  SkottieWrapper::FrameDataCallback Get() {
    return base::BindRepeating(&MockFrameDataCallback::OnAssetLoaded,
                               base::Unretained(this));
  }
};

TEST(SkottieWrapperTest, LoadsValidLottieFileNonSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, LoadsValidLottieFileSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::UnsafeCreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, DetectsInvalidLottieFile) {
  static constexpr std::string_view kInvalidJson = "this is invalid json";
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kInvalidJson.data()),
          kInvalidJson.length()));
  EXPECT_FALSE(skottie->is_valid());
}

TEST(SkottieWrapperTest, IdMatchesForSameLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::UnsafeCreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Eq(skottie_2->id()));
}

TEST(SkottieWrapperTest, IdDoesNotMatchForDifferentLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets2.data()),
          kLottieDataWithoutAssets2.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Ne(skottie_2->id()));
}

TEST(SkottieWrapperTest, LoadsImageAssetsMetadata) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::UnsafeCreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWith2Assets.data()),
          kLottieDataWith2Assets.length()));
  ASSERT_TRUE(skottie->is_valid());
  SkottieResourceMetadataMap metadata = skottie->GetImageAssetMetadata();
  EXPECT_THAT(
      metadata.asset_storage(),
      UnorderedElementsAre(
          Pair("image_0",
               FieldsAre(base::FilePath(FILE_PATH_LITERAL("images/img_0.jpg"))
                             .NormalizePathSeparators(),
                         Optional(gfx::Size(kLottieDataWith2AssetsWidth,
                                            kLottieDataWith2AssetsHeight)))),
          Pair("image_1",
               FieldsAre(base::FilePath(FILE_PATH_LITERAL("images/img_1.jpg"))
                             .NormalizePathSeparators(),
                         Optional(gfx::Size(kLottieDataWith2AssetsWidth,
                                            kLottieDataWith2AssetsHeight))))));
}

TEST(SkottieWrapperTest, LoadsCorrectAssetsForDraw) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;
  MockFrameDataCallback mock_callback;
  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_0"), _, _, _));
  skottie->Draw(&canvas, /*t=*/0.25, SkRect::MakeWH(500, 500),
                mock_callback.Get(), SkottieColorMap(),
                SkottieTextPropertyValueMap());
  Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_1"), _, _, _));
  skottie->Draw(&canvas, /*t=*/0.75, SkRect::MakeWH(500, 500),
                mock_callback.Get(), SkottieColorMap(),
                SkottieTextPropertyValueMap());
  Mock::VerifyAndClearExpectations(&mock_callback);
}

TEST(SkottieWrapperTest, AllowsNullFrameDataCallbackForDraw) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWithoutAssets1);
  ASSERT_TRUE(skottie->is_valid());
  // Just verify that this call does not cause a CHECK failure.
  ::testing::NiceMock<MockCanvas> canvas;
  skottie->Draw(&canvas, /*t=*/0, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), SkottieColorMap(),
                SkottieTextPropertyValueMap());
}

TEST(SkottieWrapperTest, LoadsCorrectAssetsForSeek) {
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;
  MockFrameDataCallback mock_callback;
  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_0"), _, _, _));
  skottie->Seek(/*t=*/0.25, mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              OnAssetLoaded(HashSkottieResourceId("image_1"), _, _, _));
  skottie->Seek(/*t=*/0.75, mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);
}

TEST(SkottieWrapperTest, LoadsColorNodes) {
  auto skottie = CreateSkottieFromString(kLottieDataWithoutAssets1);
  ASSERT_TRUE(skottie->is_valid());
  EXPECT_THAT(
      skottie->GetCurrentColorPropertyValues(),
      UnorderedElementsAre(
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color1Node),
               kLottieDataWithoutAssets1Color1),
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color2Node),
               kLottieDataWithoutAssets1Color2)));
}

TEST(SkottieWrapperTest, SetsColorNodesWithDraw) {
  auto skottie = CreateSkottieFromString(kLottieDataWithoutAssets1);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;

  SkottieColorMap color_map = {
      {HashSkottieResourceId(kLottieDataWithoutAssets1Color1Node),
       SK_ColorYELLOW},
      {HashSkottieResourceId(kLottieDataWithoutAssets1Color2Node),
       SK_ColorCYAN}};
  skottie->Draw(&canvas, /*t=*/0, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), color_map,
                SkottieTextPropertyValueMap());
  EXPECT_THAT(
      skottie->GetCurrentColorPropertyValues(),
      UnorderedElementsAre(
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color1Node),
               SK_ColorYELLOW),
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color2Node),
               SK_ColorCYAN)));

  color_map = {{HashSkottieResourceId(kLottieDataWithoutAssets1Color2Node),
                SK_ColorMAGENTA}};
  skottie->Draw(&canvas, /*t=*/0, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), color_map,
                SkottieTextPropertyValueMap());
  EXPECT_THAT(
      skottie->GetCurrentColorPropertyValues(),
      UnorderedElementsAre(
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color1Node),
               SK_ColorYELLOW),
          Pair(HashSkottieResourceId(kLottieDataWithoutAssets1Color2Node),
               SK_ColorMAGENTA)));
}

TEST(SkottieWrapperTest, LoadsTextNodes) {
  auto skottie = CreateSkottieFromTestDataDir(kLottieDataWith2TextFileName);
  ASSERT_TRUE(skottie->is_valid());
  EXPECT_THAT(skottie->GetTextNodeNames(),
              UnorderedElementsAre(kLottieDataWith2TextNode1,
                                   kLottieDataWith2TextNode2));
  EXPECT_THAT(skottie->GetCurrentTextPropertyValues(),
              UnorderedElementsAre(
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode1),
                       SkottieTextPropertyValue(
                           std::string(kLottieDataWith2TextNode1Text),
                           kLottieDataWith2TextNode1Box)),
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode2),
                       SkottieTextPropertyValue(
                           std::string(kLottieDataWith2TextNode2Text),
                           kLottieDataWith2TextNode2Box))));
}

TEST(SkottieWrapperTest, SetsTextNodesWithDraw) {
  auto skottie = CreateSkottieFromTestDataDir(kLottieDataWith2TextFileName);
  ASSERT_TRUE(skottie->is_valid());
  ::testing::NiceMock<MockCanvas> canvas;

  SkottieTextPropertyValueMap text_map = {
      {HashSkottieResourceId(kLottieDataWith2TextNode1),
       SkottieTextPropertyValue("new-test-text-1", gfx::RectF(1, 1, 100, 100))},
      {HashSkottieResourceId(kLottieDataWith2TextNode2),
       SkottieTextPropertyValue("new-test-text-2",
                                gfx::RectF(2, 2, 200, 200))}};
  skottie->Draw(&canvas, /*t=*/0, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), SkottieColorMap(),
                text_map);
  EXPECT_THAT(skottie->GetCurrentTextPropertyValues(),
              UnorderedElementsAre(
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode1),
                       SkottieTextPropertyValue("new-test-text-1",
                                                gfx::RectF(1, 1, 100, 100))),
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode2),
                       SkottieTextPropertyValue("new-test-text-2",
                                                gfx::RectF(2, 2, 200, 200)))));
  // Check that we've actually drawn some text.
  EXPECT_CALL(canvas, onDrawGlyphRunList).Times(AtLeast(1));

  text_map = {{HashSkottieResourceId(kLottieDataWith2TextNode2),
               SkottieTextPropertyValue("new-test-text-2b",
                                        gfx::RectF(3, 3, 300, 300))}};
  skottie->Draw(&canvas, /*t=*/0.1, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), SkottieColorMap(),
                text_map);
  EXPECT_THAT(skottie->GetCurrentTextPropertyValues(),
              UnorderedElementsAre(
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode1),
                       SkottieTextPropertyValue("new-test-text-1",
                                                gfx::RectF(1, 1, 100, 100))),
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode2),
                       SkottieTextPropertyValue("new-test-text-2b",
                                                gfx::RectF(3, 3, 300, 300)))));

  // Missing glyphs should not trigger a crash.
  text_map = {
      {HashSkottieResourceId(kLottieDataWith2TextNode1),
       SkottieTextPropertyValue("hello 你好", gfx::RectF(4, 4, 400, 400))}};
  skottie->Draw(&canvas, /*t=*/0.2, SkRect::MakeWH(500, 500),
                SkottieWrapper::FrameDataCallback(), SkottieColorMap(),
                text_map);
  EXPECT_THAT(skottie->GetCurrentTextPropertyValues(),
              UnorderedElementsAre(
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode1),
                       SkottieTextPropertyValue("hello 你好",
                                                gfx::RectF(4, 4, 400, 400))),
                  Pair(HashSkottieResourceId(kLottieDataWith2TextNode2),
                       SkottieTextPropertyValue("new-test-text-2b",
                                                gfx::RectF(3, 3, 300, 300)))));
}

TEST(SkottieWrapperTest, Marker) {
  auto skottie = CreateSkottieFromString(kLottieDataWith2Markers);
  ASSERT_TRUE(skottie->is_valid());
  EXPECT_THAT(
      skottie->GetAllMarkers(),
      UnorderedElementsAre(
          FieldsAre(
              kLottieDataWith2MarkersMarker1,
              FloatNear(kLottieDataWith2MarkersMarker1Time, kMarkerEpsilon),
              FloatNear(kLottieDataWith2MarkersMarker1Time, kMarkerEpsilon)),
          FieldsAre(
              kLottieDataWith2MarkersMarker2,
              FloatNear(kLottieDataWith2MarkersMarker2Time, kMarkerEpsilon),
              FloatNear(kLottieDataWith2MarkersMarker2Time, kMarkerEpsilon))));
}

TEST(SkottieWrapperTest, LoadsTransformNodes) {
  auto skottie = CreateSkottieFromTestDataDir(kLottieDataWith2TextFileName);
  ASSERT_TRUE(skottie->is_valid());
  EXPECT_THAT(skottie->GetTextNodeNames(),
              UnorderedElementsAre(kLottieDataWith2TextNode1,
                                   kLottieDataWith2TextNode2));
  EXPECT_THAT(
      skottie->GetCurrentTransformPropertyValues(),
      IsSupersetOf({Pair(HashSkottieResourceId(kLottieDataWith2TextNode1),
                         SkottieTransformPropertyValue(
                             {kLottieDataWith2TextNode1Position})),
                    Pair(HashSkottieResourceId(kLottieDataWith2TextNode2),
                         SkottieTransformPropertyValue(
                             {kLottieDataWith2TextNode2Position}))}));
}

}  // namespace
}  // namespace cc

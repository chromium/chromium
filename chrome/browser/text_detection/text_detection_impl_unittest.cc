// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/text_detection/text_detection_impl.h"

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace text_detection {

namespace {

SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorWHITE);
  return bitmap;
}

}  // namespace

class TextDetectionImplTest : public ChromeRenderViewHostTestHarness {
 public:
  TextDetectionImplTest() = default;
  ~TextDetectionImplTest() override = default;
};

TEST_F(TextDetectionImplTest, EmptyBitmapReturnsEmptyResults) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line = screen_ai::mojom::LineBox::New();
  line->text_line = "Should Not Be Returned";
  line->bounding_box = gfx::Rect(10, 20, 100, 30);
  annotation->lines.push_back(std::move(line));
  fake_ocr->set_visual_annotation_result(std::move(annotation));

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl* impl = TextDetectionImpl::CreateForTesting(
      main_rfh(), fake_ocr, remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      future;
  // Pass an empty bitmap directly to `Detect()` (bypassing Mojo serialization,
  // which rejects empty bitmaps) to trigger the `bitmap.drawsNothing()`
  // early-return guard before `PerformOCR()` is called.
  impl->Detect(SkBitmap(), future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(TextDetectionImplTest, NoTextDetectedReturnsEmptyResults) {
  // Without `set_visual_annotation_result()`, `FakeOpticalCharacterRecognizer`
  // returns an empty `VisualAnnotation` by default.
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      future;
  remote->Detect(CreateTestBitmap(/*width=*/100, /*height=*/100),
                 future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(TextDetectionImplTest, ConvertsVisualAnnotationCorrectly) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);

  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto empty_line = screen_ai::mojom::LineBox::New();
  empty_line->bounding_box = gfx::Rect(0, 0, 50, 10);
  annotation->lines.push_back(std::move(empty_line));

  auto line = screen_ai::mojom::LineBox::New();
  line->text_line = "Hello World";
  line->bounding_box = gfx::Rect(10, 20, 100, 30);
  line->bounding_box_angle = std::numeric_limits<float>::quiet_NaN();
  annotation->lines.push_back(std::move(line));

  fake_ocr->set_visual_annotation_result(std::move(annotation));

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      future;
  remote->Detect(CreateTestBitmap(/*width=*/200, /*height=*/200),
                 future.GetCallback());

  const auto& results = future.Get();
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0]->raw_value, "Hello World");
  EXPECT_EQ(results[0]->bounding_box, gfx::RectF(10.0f, 20.0f, 100.0f, 30.0f));
  ASSERT_EQ(results[0]->corner_points.size(), 4u);
  EXPECT_EQ(results[0]->corner_points[0], gfx::PointF(10.0f, 20.0f));
  EXPECT_EQ(results[0]->corner_points[1], gfx::PointF(110.0f, 20.0f));
  EXPECT_EQ(results[0]->corner_points[2], gfx::PointF(110.0f, 50.0f));
  EXPECT_EQ(results[0]->corner_points[3], gfx::PointF(10.0f, 50.0f));
}

TEST_F(TextDetectionImplTest, HandlesRotatedText) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);

  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line = screen_ai::mojom::LineBox::New();
  line->text_line = "Rotated Text";
  line->bounding_box = gfx::Rect(50, 50, 100, 20);
  line->bounding_box_angle = 90.0f;
  annotation->lines.push_back(std::move(line));

  fake_ocr->set_visual_annotation_result(std::move(annotation));

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      future;
  remote->Detect(CreateTestBitmap(/*width=*/200, /*height=*/200),
                 future.GetCallback());

  const auto& results = future.Get();
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0]->raw_value, "Rotated Text");
  ASSERT_EQ(results[0]->corner_points.size(), 4u);
  // Rotating the 100x20 box 90 degrees clockwise around the top-left pivot
  // [0] (50,50) in screen coordinates (+X right, +Y down):
  //
  //   Before rotation (x=50, y=50, w=100, h=20):
  //          50                  150
  //     50   [0]-----------------+ [1] (150,50)
  //          |      100x20       |
  //     70   [3]-----------------+ [2] (150,70)
  //        (50,70)
  //
  //   After 90-deg clockwise rotation around [0] (x=30, y=50, w=20, h=100):
  //          30     50
  //     50   [3']---[0'] (50,50)
  //          |      |
  //          | 20x  |
  //          | 100  |
  //    150   [2']---[1'] (50,150)
  //        (30,150)
  EXPECT_FLOAT_EQ(results[0]->corner_points[0].x(), 50.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[0].y(), 50.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[1].x(), 50.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[1].y(), 150.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[2].x(), 30.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[2].y(), 150.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[3].x(), 30.0f);
  EXPECT_FLOAT_EQ(results[0]->corner_points[3].y(), 50.0f);

  EXPECT_FLOAT_EQ(results[0]->bounding_box.x(), 30.0f);
  EXPECT_FLOAT_EQ(results[0]->bounding_box.y(), 50.0f);
  EXPECT_FLOAT_EQ(results[0]->bounding_box.width(), 20.0f);
  EXPECT_FLOAT_EQ(results[0]->bounding_box.height(), 100.0f);
}

TEST_F(TextDetectionImplTest, EnsureReadyReturnsTrueWhenReady) {
  // `FakeOpticalCharacterRecognizer` initializes `ready_` to `true` by default.
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future;
  remote->EnsureReady(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(TextDetectionImplTest, EnsureReadyReturnsFalseWhenInitializationFails) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  fake_ocr->set_ready(/*ready=*/false);

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future;
  remote->EnsureReady(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(TextDetectionImplTest,
       PendingRequestsQueuedUntilOcrInitializationSucceeds) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  fake_ocr->set_ready(/*ready=*/std::nullopt);

  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line = screen_ai::mojom::LineBox::New();
  line->text_line = "Queued Text";
  line->bounding_box = gfx::Rect(10, 10, 50, 20);
  annotation->lines.push_back(std::move(line));
  fake_ocr->set_visual_annotation_result(std::move(annotation));

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl* impl = TextDetectionImpl::CreateForTesting(
      main_rfh(), fake_ocr, remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> ready_future;
  remote->EnsureReady(ready_future.GetCallback());
  EXPECT_FALSE(ready_future.IsReady());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      detect_future;
  remote->Detect(CreateTestBitmap(/*width=*/100, /*height=*/100),
                 detect_future.GetCallback());
  EXPECT_FALSE(detect_future.IsReady());

  fake_ocr->set_ready(/*ready=*/true);
  impl->SimulateOcrInitializationForTesting(/*successful=*/true);

  EXPECT_TRUE(ready_future.Get());
  const auto& results = detect_future.Get();
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0]->raw_value, "Queued Text");
}

TEST_F(TextDetectionImplTest,
       PendingRequestsResolvedWithEmptyOnOcrInitializationFailure) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  fake_ocr->set_ready(/*ready=*/std::nullopt);

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl* impl = TextDetectionImpl::CreateForTesting(
      main_rfh(), fake_ocr, remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> ready_future;
  remote->EnsureReady(ready_future.GetCallback());
  EXPECT_FALSE(ready_future.IsReady());

  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      detect_future;
  remote->Detect(CreateTestBitmap(/*width=*/100, /*height=*/100),
                 detect_future.GetCallback());
  EXPECT_FALSE(detect_future.IsReady());

  fake_ocr->set_ready(/*ready=*/false);
  impl->SimulateOcrInitializationForTesting(/*successful=*/false);

  EXPECT_FALSE(ready_future.Get());
  EXPECT_TRUE(detect_future.Get().empty());
}

TEST_F(TextDetectionImplTest, ExceedingMaxPendingRequestsReturnsEmpty) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  fake_ocr->set_defer_perform_ocr(/*defer=*/true);

  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line = screen_ai::mojom::LineBox::New();
  line->text_line = "Active Text";
  line->bounding_box = gfx::Rect(10, 10, 50, 20);
  annotation->lines.push_back(std::move(line));
  fake_ocr->set_visual_annotation_result(std::move(annotation));

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  std::vector<base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>>
      active_futures(TextDetectionImpl::kMaxPendingRequests);
  for (size_t i = 0; i < TextDetectionImpl::kMaxPendingRequests; ++i) {
    remote->Detect(CreateTestBitmap(/*width=*/100, /*height=*/100),
                   active_futures[i].GetCallback());
  }

  // When `kMaxPendingRequests` requests are already in flight to PerformOCR(),
  // the next Detect() request currently returns empty results.
  // TODO(crbug.com/570081637): Update the behavior and this test once the spec
  // defines which error type to return.
  base::test::TestFuture<
      std::vector<shape_detection::mojom::TextDetectionResultPtr>>
      overflow_future;
  remote->Detect(CreateTestBitmap(/*width=*/100, /*height=*/100),
                 overflow_future.GetCallback());
  EXPECT_TRUE(overflow_future.Get().empty());

  for (size_t i = 0; i < TextDetectionImpl::kMaxPendingRequests; ++i) {
    EXPECT_FALSE(active_futures[i].IsReady());
  }

  fake_ocr->FlushDeferredPerformOcr();

  for (size_t i = 0; i < TextDetectionImpl::kMaxPendingRequests; ++i) {
    const auto& results = active_futures[i].Get();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0]->raw_value, "Active Text");
  }
}

TEST_F(TextDetectionImplTest, ConcurrentEnsureReadyCallRejectedWhilePending) {
  auto fake_ocr = screen_ai::FakeOpticalCharacterRecognizer::Create(
      /*empty_ax_tree_update_result=*/false);
  fake_ocr->set_ready(/*ready=*/std::nullopt);

  mojo::Remote<shape_detection::mojom::TextDetection> remote;
  TextDetectionImpl::CreateForTesting(main_rfh(), fake_ocr,
                                      remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  base::test::TestFuture<bool> first_ready_future;
  remote->EnsureReady(first_ready_future.GetCallback());

  base::test::TestFuture<bool> second_ready_future;
  remote->EnsureReady(second_ready_future.GetCallback());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Unexpected duplicate EnsureReady() request.");
}

}  // namespace text_detection

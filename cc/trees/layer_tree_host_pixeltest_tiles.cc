// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/test/buildflags.h"
#include "gpu/command_buffer/client/raster_interface.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostTilesPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<RasterTestConfig> {
 protected:
  LayerTreeHostTilesPixelTest() : LayerTreePixelTest(renderer_type()) {
    set_raster_type(GetParam().raster_type);
  }

  viz::RendererType renderer_type() const { return GetParam().renderer_type; }

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreePixelTest::InitializeSettings(settings);

    settings->use_partial_raster = use_partial_raster_;
  }

  void BeginTest() override {
    // Don't set up a readback target at the start of the test.
    PostSetNeedsCommitToMainThread();
  }

  void DoReadback() {
    Layer* target =
        readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  }

  base::FilePath ref_file_;
  std::unique_ptr<SkBitmap> result_bitmap_;
  bool use_partial_raster_ = false;
};

class BlueYellowClient : public ContentLayerClient {
 public:
  explicit BlueYellowClient(const gfx::Size& size)
      : size_(size), blue_top_(true) {}

  gfx::Rect PaintableRegion() override { return gfx::Rect(size_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_status) override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();

    display_list->StartPaint();

    gfx::Rect top(0, 0, size_.width(), size_.height() / 2);
    gfx::Rect bottom(0, size_.height() / 2, size_.width(), size_.height() / 2);

    gfx::Rect blue_rect = blue_top_ ? top : bottom;
    gfx::Rect yellow_rect = blue_top_ ? bottom : top;

    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);

    // Use custom colors with 0xF2 rather than the default blue/yellow (which
    // use 0xFF), as the default won't show dither patterns as it exactly maps
    // to a 16-bit color.
    flags.setColor(SkColorSetRGB(0x00, 0x00, 0xF2));
    display_list->push<DrawRectOp>(gfx::RectToSkRect(blue_rect), flags);
    flags.setColor(SkColorSetRGB(0xF2, 0xF2, 0x00));
    display_list->push<DrawRectOp>(gfx::RectToSkRect(yellow_rect), flags);

    display_list->EndPaintOfUnpaired(PaintableRegion());
    display_list->Finalize();
    return display_list;
  }

  bool FillsBoundsCompletely() const override { return true; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

  void set_blue_top(bool b) { blue_top_ = b; }

 private:
  gfx::Size size_;
  bool blue_top_;
};

class LayerTreeHostTilesTestPartialInvalidation
    : public LayerTreeHostTilesPixelTest {
 public:
  LayerTreeHostTilesTestPartialInvalidation()
      : client_(gfx::Size(200, 200)),
        picture_layer_(PictureLayer::Create(&client_)) {
    picture_layer_->SetBounds(gfx::Size(200, 200));
    picture_layer_->SetIsDrawable(true);
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // We have done one frame, but the resource may not be available for
        // partial raster yet. Force a second frame.
        picture_layer_->SetNeedsDisplayRect(gfx::Rect(50, 50, 100, 100));
        break;
      case 2:
        // We have done two frames, so the layer's content has been rastered
        // twice and the first frame's resource is available for partial
        // raster. Now we change the picture behind it to record something
        // completely different, but we give a smaller invalidation rect. The
        // layer should only re-raster the stuff in the rect. If it doesn't do
        // partial raster it would re-raster the whole thing instead.
        client_.set_blue_top(false);
        Finish();
        picture_layer_->SetNeedsDisplayRect(gfx::Rect(50, 50, 100, 100));

        // Add a copy request to see what happened!
        DoReadback();
        break;
    }
  }

  void WillPrepareTilesOnThread(LayerTreeHostImpl* host_impl) override {
    // Issue a GL finish before preparing tiles to ensure resources become
    // available for use in a timely manner. Needed for the one-copy path.
    viz::RasterContextProvider* context_provider =
        host_impl->layer_tree_frame_sink()->worker_context_provider();
    if (!context_provider)
      return;

    viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
    lock.RasterInterface()->Finish();
  }

 protected:
  BlueYellowClient client_;
  scoped_refptr<PictureLayer> picture_layer_;
};

std::vector<RasterTestConfig> const kTestCases = {
    {viz::RendererType::kSoftware, TestRasterType::kBitmap},
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    {viz::RendererType::kGL, TestRasterType::kOneCopy},
    {viz::RendererType::kGL, TestRasterType::kGpu},
    {viz::RendererType::kSkiaGL, TestRasterType::kOneCopy},
    {viz::RendererType::kSkiaGL, TestRasterType::kGpu},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    {viz::RendererType::kSkiaVk, TestRasterType::kOop},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_DAWN_BACKEND_TESTS)
    {viz::RendererType::kSkiaDawn, TestRasterType::kOop},
#endif  // BUILDFLAG(ENABLE_DAWN_BACKEND_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestPartialInvalidation,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

#if defined(OS_CHROMEOS) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(OS_FUCHSIA)
// TODO(crbug.com/1045521): Flakes on all slower bots.
#define MAYBE_PartialRaster DISABLED_PartialRaster
#else
#define MAYBE_PartialRaster PartialRaster
#endif
TEST_P(LayerTreeHostTilesTestPartialInvalidation, MAYBE_PartialRaster) {
  use_partial_raster_ = true;
  RunSingleThreadedPixelTest(
      picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}
#undef MAYBE_PartialRaster

TEST_P(LayerTreeHostTilesTestPartialInvalidation, FullRaster) {
  RunSingleThreadedPixelTest(
      picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

std::vector<RasterTestConfig> const kTestCasesMultiThread = {
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    {viz::RendererType::kGL, TestRasterType::kOneCopy},
    {viz::RendererType::kSkiaGL, TestRasterType::kOneCopy},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    // TODO(sgilhuly): Switch this to one copy raster once is is supported for
    // Vulkan in these tests.
    {viz::RendererType::kSkiaVk, TestRasterType::kOop},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_DAWN_BACKEND_TESTS)
    {viz::RendererType::kSkiaDawn, TestRasterType::kOop},
#endif  // BUILDFLAG(ENABLE_DAWN_BACKEND_TESTS)
};

using LayerTreeHostTilesTestPartialInvalidationMultiThread =
    LayerTreeHostTilesTestPartialInvalidation;

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestPartialInvalidationMultiThread,
                         ::testing::ValuesIn(kTestCasesMultiThread),
                         ::testing::PrintToStringParamName());

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(THREAD_SANITIZER)
// Flaky on Linux TSAN. https://crbug.com/707711
#define MAYBE_PartialRaster DISABLED_PartialRaster
#elif defined(OS_CHROMEOS) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(OS_FUCHSIA)
// TODO(crbug.com/1045521): Flakes on all slower bots.
#define MAYBE_PartialRaster DISABLED_PartialRaster
#else
#define MAYBE_PartialRaster PartialRaster
#endif
TEST_P(LayerTreeHostTilesTestPartialInvalidationMultiThread,
       MAYBE_PartialRaster) {
  use_partial_raster_ = true;
  RunPixelTest(
      picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}
#undef MAYBE_PartialRaster

TEST_P(LayerTreeHostTilesTestPartialInvalidationMultiThread, FullRaster) {
  RunPixelTest(picture_layer_,
               base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

// This test doesn't work on Vulkan because on our hardware we can't render to
// RGBA4444 format using either SwiftShader or native Vulkan. See
// crbug.com/987278 for details
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
class LayerTreeHostTilesTestPartialInvalidationLowBitDepth
    : public LayerTreeHostTilesTestPartialInvalidation {
 protected:
  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostTilesPixelTest::InitializeSettings(settings);
    settings->use_rgba_4444 = true;
    settings->unpremultiply_and_dither_low_bit_depth_tiles = true;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerTreeHostTilesTestPartialInvalidationLowBitDepth,
    ::testing::Values(
        RasterTestConfig{viz::RendererType::kSkiaGL, TestRasterType::kGpu},
        RasterTestConfig{viz::RendererType::kGL, TestRasterType::kGpu}),
    ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostTilesTestPartialInvalidationLowBitDepth, PartialRaster) {
  use_partial_raster_ = true;
  RunSingleThreadedPixelTest(picture_layer_,
                             base::FilePath(FILE_PATH_LITERAL(
                                 "blue_yellow_partial_flipped_dither.png")));
}

TEST_P(LayerTreeHostTilesTestPartialInvalidationLowBitDepth, FullRaster) {
  RunSingleThreadedPixelTest(
      picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped_dither.png")));
}
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)

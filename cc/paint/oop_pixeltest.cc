// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/completion_event.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/raster/playback_image_provider.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/buildflags.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "ipc/common/gpu_client_ids.h"
#include "skia/ext/legacy_display_globals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_implementation.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace cc {
namespace {
scoped_refptr<DisplayItemList> MakeNoopDisplayItemList() {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<SaveOp>();
  display_item_list->push<RestoreOp>();
  display_item_list->EndPaintOfUnpaired(gfx::Rect(10000, 10000));
  display_item_list->Finalize();
  return display_item_list;
}

// Creates a bitmap of |size| filled with pixels of |color|.
SkBitmap MakeSolidColorBitmap(gfx::Size size, SkColor4f color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size.width(), size.height()));
  bitmap.eraseColor(color);
  return bitmap;
}

// Creates a SkImage filled with magenta and a 30x40 green rectangle.
sk_sp<SkImage> MakeSkImage(const gfx::Size& size,
                           sk_sp<SkColorSpace> color_space = nullptr) {
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(size.width(), size.height(), color_space),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SkColors::kMagenta);
  SkPaint green;
  green.setColor(SkColors::kGreen);
  canvas.drawRect(SkRect::MakeXYWH(10, 20, 30, 40), green);

  return SkImage::MakeFromBitmap(bitmap);
}

constexpr size_t kCacheLimitBytes = 1024 * 1024;
constexpr PaintFlags::FilterQuality kDefaultFilterQuality =
    PaintFlags::FilterQuality::kNone;

class OopPixelTest : public testing::Test,
                     public gpu::raster::GrShaderCache::Client {
 public:
  OopPixelTest() : gr_shader_cache_(kCacheLimitBytes, this) {}

  void SetUp() override { InitializeOOPContext(); }

  // gpu::raster::GrShaderCache::Client implementation.
  void StoreShader(const std::string& key, const std::string& shader) override {
  }

  void InitializeOOPContext() {
    if (oop_image_cache_)
      oop_image_cache_.reset();

    raster_context_provider_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            viz::TestContextType::kGpuRaster, /*support_locking=*/false,
            &gr_shader_cache_, &activity_flags_);
    gpu::ContextResult result =
        raster_context_provider_->BindToCurrentSequence();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    const int raster_max_texture_size =
        raster_context_provider_->ContextCapabilities().max_texture_size;
    oop_image_cache_ = std::make_unique<GpuImageDecodeCache>(
        raster_context_provider_.get(), true, kRGBA_8888_SkColorType,
        kWorkingSetSize, raster_max_texture_size, nullptr);
  }

  class RasterOptions {
   public:
    RasterOptions() = default;
    explicit RasterOptions(const gfx::Size& playback_size) {
      resource_size = playback_size;
      content_size = resource_size;
      full_raster_rect = gfx::Rect(playback_size);
      playback_rect = gfx::Rect(playback_size);
    }

    SkColor4f background_color = SkColors::kBlack;
    int msaa_sample_count = 0;
    bool use_lcd_text = false;
    PlaybackImageProvider::RasterMode image_provider_raster_mode =
        PlaybackImageProvider::RasterMode::kSoftware;
    gfx::Size resource_size;
    gfx::Size content_size;
    gfx::Rect full_raster_rect;
    gfx::Rect playback_rect;
    gfx::Vector2dF post_translate = {0.f, 0.f};
    float post_scale = 1.f;
    TargetColorParams target_color_params;
    bool requires_clear = false;
    bool preclear = false;
    SkColor4f preclear_color;
    raw_ptr<ImageDecodeCache> image_cache = nullptr;
    std::vector<scoped_refptr<DisplayItemList>> additional_lists;
    raw_ptr<PaintShader> shader_with_animated_images = nullptr;
  };

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const gfx::Size& playback_size) {
    RasterOptions options(playback_size);
    return Raster(display_item_list, options);
  }

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const RasterOptions& options) {
    absl::optional<PlaybackImageProvider::Settings> settings;
    settings.emplace(PlaybackImageProvider::Settings());
    settings->raster_mode = options.image_provider_raster_mode;
    PlaybackImageProvider image_provider(oop_image_cache_.get(),
                                         options.target_color_params,
                                         std::move(settings));

    int width = options.resource_size.width();
    int height = options.resource_size.height();

    // Create and allocate a shared image on the raster interface.
    auto* ri = raster_context_provider_->RasterInterface();
    auto* sii = raster_context_provider_->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        viz::ResourceFormat::RGBA_8888, gfx::Size(width, height),
        options.target_color_params.color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, flags, gpu::kNullSurfaceHandle);
    EXPECT_TRUE(mailbox.Verify());
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    // Assume legacy MSAA if sample count is positive.
    gpu::raster::MsaaMode msaa_mode = options.msaa_sample_count > 0
                                          ? gpu::raster::kMSAA
                                          : gpu::raster::kNoMSAA;

    if (options.preclear) {
      ri->BeginRasterCHROMIUM(
          options.preclear_color,
          /*needs_clear=*/options.preclear, options.msaa_sample_count,
          msaa_mode, options.use_lcd_text,
          /*visible=*/true, options.target_color_params.color_space,
          mailbox.name);
      ri->EndRasterCHROMIUM();
    }

    // "Out of process" raster! \o/
    // If |options.preclear| is true, the mailbox has already been cleared by
    // the BeginRasterCHROMIUM call above, and we want to test that it is indeed
    // cleared, so set |needs_clear| to false here.
    ri->BeginRasterCHROMIUM(
        options.background_color,
        /*needs_clear=*/!options.preclear, options.msaa_sample_count, msaa_mode,
        options.use_lcd_text,
        /*visible=*/true, options.target_color_params.color_space,
        mailbox.name);
    size_t max_op_size_limit =
        gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
    ri->RasterCHROMIUM(display_item_list.get(), &image_provider,
                       options.content_size, options.full_raster_rect,
                       options.playback_rect, options.post_translate,
                       gfx::Vector2dF(options.post_scale, options.post_scale),
                       options.requires_clear, &max_op_size_limit);
    for (const auto& list : options.additional_lists) {
      ri->RasterCHROMIUM(list.get(), &image_provider, options.content_size,
                         options.full_raster_rect, options.playback_rect,
                         options.post_translate,
                         gfx::Vector2dF(options.post_scale, options.post_scale),
                         options.requires_clear, &max_op_size_limit);
    }
    ri->EndRasterCHROMIUM();
    ri->OrderingBarrierCHROMIUM();

    EXPECT_EQ(ri->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    SkBitmap result = ReadbackMailbox(ri, mailbox, options.resource_size);
    gpu::SyncToken sync_token;
    ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    sii->DestroySharedImage(sync_token, mailbox);
    return result;
  }

  SkBitmap ReadbackMailbox(gpu::raster::RasterInterface* ri,
                           const gpu::Mailbox& mailbox,
                           const gfx::Size& image_size,
                           sk_sp<SkColorSpace> color_space = nullptr) {
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(
        image_size.width(), image_size.height(), color_space);
    SkBitmap result;
    result.allocPixels(image_info);
    ri->ReadbackImagePixels(mailbox, image_info, image_info.minRowBytes(), 0, 0,
                            result.getPixels());
    return result;
  }

  gpu::Mailbox CreateMailboxSharedImage(
      gpu::raster::RasterInterface* ri,
      gpu::SharedImageInterface* sii,
      const RasterOptions& options,
      viz::ResourceFormat image_format,
      absl::optional<gfx::ColorSpace> color_space = absl::nullopt) {
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        image_format, options.resource_size,
        color_space.value_or(options.target_color_params.color_space),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, flags,
        gpu::kNullSurfaceHandle);
    EXPECT_TRUE(mailbox.Verify());
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    return mailbox;
  }

  void UploadPixels(gpu::raster::RasterInterface* ri,
                    const gpu::Mailbox& mailbox,
                    const SkImageInfo& info,
                    const SkBitmap& bitmap) {
    ri->WritePixels(mailbox, 0, 0, 0, info.minRowBytes(), info,
                    bitmap.getPixels());
    ri->OrderingBarrierCHROMIUM();
    EXPECT_EQ(ri->GetError(), static_cast<unsigned>(GL_NO_ERROR));
  }

  // Verifies |actual| matches the expected PNG image.
  void ExpectEquals(const SkBitmap& actual,
                    const base::FilePath::StringType& ref_filename,
                    const PixelComparator& comparator =
                        ExactPixelComparator(/*discard_alpha=*/false)) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir));

    base::FilePath png_path = test_data_dir.Append(ref_filename);

    auto* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(switches::kRebaselinePixelTests)) {
      EXPECT_TRUE(WritePNGFile(actual, png_path, true));
    } else {
      EXPECT_TRUE(MatchesPNGFile(actual, png_path, comparator));
    }
  }

  void ExpectEquals(SkBitmap actual,
                    SkBitmap expected,
                    const PixelComparator& comparator =
                        ExactPixelComparator(/*discard_alpha=*/false)) {
    EXPECT_TRUE(MatchesBitmap(actual, expected, comparator));
  }

 protected:
  static constexpr size_t kWorkingSetSize = 64 * 1024 * 1024;
  scoped_refptr<viz::TestInProcessContextProvider> raster_context_provider_;
  std::unique_ptr<GpuImageDecodeCache> oop_image_cache_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<ImageProvider> image_provider_;
  int color_space_id_ = 0;
  gpu::raster::GrShaderCache gr_shader_cache_;
  gpu::GpuProcessActivityFlags activity_flags_;
};

class OopClearPixelTest : public OopPixelTest,
                          public ::testing::WithParamInterface<bool> {
 public:
  bool IsPartialRaster() const { return GetParam(); }
};

TEST_F(OopPixelTest, DrawColor) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  SkBitmap expected = MakeSolidColorBitmap(rect.size(), SkColors::kBlue);

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawColorWithTargetColorSpace) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  gfx::ColorSpace target_color_space = gfx::ColorSpace::CreateXYZD50();

  RasterOptions options(rect.size());
  options.target_color_params.color_space = target_color_space;

  SkBitmap expected = MakeSolidColorBitmap(
      rect.size(), SkColor4f::FromColor(SkColorSetARGB(255, 38, 15, 221)));

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRect) {
  gfx::Rect rect(10, 10);
  auto color_paint = [](int r, int g, int b) {
    PaintFlags flags;
    flags.setColor(SkColorSetARGB(255, r, g, b));
    return flags;
  };
  std::vector<std::pair<SkRect, PaintFlags>> input = {
      {SkRect::MakeXYWH(0, 0, 5, 5), color_paint(0, 0, 255)},
      {SkRect::MakeXYWH(5, 0, 5, 5), color_paint(0, 255, 0)},
      {SkRect::MakeXYWH(0, 5, 5, 5), color_paint(0, 255, 255)},
      {SkRect::MakeXYWH(5, 5, 5, 5), color_paint(255, 0, 0)}};

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  for (auto& op : input) {
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(op.first, op.second);
    display_item_list->EndPaintOfUnpaired(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(op.first)));
  }
  display_item_list->Finalize();

  auto actual = Raster(std::move(display_item_list), rect.size());

  // Expected colors are 5x5 rects of
  //  BLUE GREEN
  //  CYAN  RED
  std::vector<SkPMColor> expected_pixels(rect.width() * rect.height());
  for (int h = 0; h < rect.height(); ++h) {
    auto start = expected_pixels.begin() + h * rect.width();
    SkPMColor left_color = SkPreMultiplyColor(
        h < 5 ? input[0].second.getColor() : input[2].second.getColor());
    SkPMColor right_color = SkPreMultiplyColor(
        h < 5 ? input[1].second.getColor() : input[3].second.getColor());

    std::fill(start, start + 5, left_color);
    std::fill(start + 5, start + 10, right_color);
  }
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
      expected_pixels.data(), rect.width() * sizeof(SkPMColor));
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRecordPaintFilterTranslatedBounds) {
  gfx::Size output_size(10, 10);

  // The paint record filter's ops would fill the right half of the image with
  // green, but its record bounds are configured to clip it to the bottom right
  // quarter of the output.
  PaintFlags internal_flags;
  internal_flags.setColor(SkColors::kGreen);
  sk_sp<PaintOpBuffer> filter_buffer(new PaintOpBuffer);
  filter_buffer->push<DrawRectOp>(
      SkRect::MakeLTRB(output_size.width() / 2.f, 0.f, output_size.width(),
                       output_size.height()),
      internal_flags);
  sk_sp<RecordPaintFilter> record_filter = sk_make_sp<RecordPaintFilter>(
      filter_buffer,
      SkRect::MakeLTRB(output_size.width() / 2.f, output_size.height() / 2.f,
                       output_size.width(), output_size.height()));

  PaintFlags record_flags;
  record_flags.setImageFilter(record_filter);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SkColors::kWhite, SkBlendMode::kSrc);
  display_item_list->push<SaveLayerOp>(nullptr, &record_flags);
  display_item_list->push<RestoreOp>();
  display_item_list->EndPaintOfUnpaired(gfx::Rect(output_size));
  display_item_list->Finalize();

  SkImageInfo ii =
      SkImageInfo::MakeN32Premul(output_size.width(), output_size.height());
  SkBitmap expected;
  expected.allocPixels(ii, ii.minRowBytes());
  expected.eraseColor(SkColors::kWhite);
  expected.erase(
      SkColors::kGreen.toSkColor(),
      SkIRect::MakeLTRB(output_size.width() / 2, output_size.height() / 2,
                        output_size.width(), output_size.height()));

  auto actual = Raster(display_item_list, output_size);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawImage) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));

  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_image.png"));
}

TEST_F(OopPixelTest, DrawImageScaled) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_image_scaled.png"));
}

TEST_F(OopPixelTest, DrawImageShaderScaled) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();
  auto paint_image_shader = PaintShader::MakeImage(
      paint_image, SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  PaintFlags flags;
  flags.setShader(paint_image_shader);
  flags.setFilterQuality(kDefaultFilterQuality);
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(rect), flags);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_image_shader_scaled.png"));
}

TEST_F(OopPixelTest, DrawRecordShaderWithImageScaled) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();
  auto paint_record = sk_make_sp<PaintOpBuffer>();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  paint_record->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling, nullptr);
  auto paint_record_shader = PaintShader::MakePaintRecord(
      paint_record, gfx::RectToSkRect(rect), SkTileMode::kRepeat,
      SkTileMode::kRepeat, nullptr);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  PaintFlags raster_flags;
  raster_flags.setShader(paint_record_shader);
  raster_flags.setFilterQuality(kDefaultFilterQuality);
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(rect), raster_flags);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_record_shader.png"));
}

TEST_F(OopPixelTest, DrawRecordShaderTranslatedTileRect) {
  auto paint_record = sk_make_sp<PaintOpBuffer>();

  // Arbitrary offsets.  The DrawRectOp inside the PaintShader draws
  // with this offset, but the tile rect also has this offset, so they
  // should cancel out, and it should be as if the DrawRectOp was at the
  // origin.
  int x_offset = 3901;
  int y_offset = -234;

  // Shader here is a tiled 2x3 rectangle with a 1x2 green block in the
  // upper left and a 10pixel wide right/bottom border.  The shader
  // tiling starts from the origin, so starting at 2,1 in the offset_rect
  // below cuts off part of that, leaving two green i's.
  PaintFlags internal_flags;
  internal_flags.setColor(SkColors::kGreen);
  sk_sp<PaintOpBuffer> shader_buffer(new PaintOpBuffer);
  shader_buffer->push<DrawRectOp>(SkRect::MakeXYWH(x_offset, y_offset, 1, 2),
                                  internal_flags);

  SkRect tile_rect = SkRect::MakeXYWH(x_offset, y_offset, 2, 3);
  sk_sp<PaintShader> paint_record_shader = PaintShader::MakePaintRecord(
      shader_buffer, tile_rect, SkTileMode::kRepeat, SkTileMode::kRepeat,
      nullptr, PaintShader::ScalingBehavior::kRasterAtScale);
  // Force paint_flags to convert this to kFixedScale, so we can safely compare
  // pixels between direct and oop-r modes (since oop will convert to
  // kFixedScale no matter what.
  paint_record_shader->set_has_animated_images(true);

  gfx::Size output_size(10, 10);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SkColors::kWhite, SkBlendMode::kSrc);
  display_item_list->push<ScaleOp>(2.f, 2.f);
  PaintFlags raster_flags;
  raster_flags.setShader(paint_record_shader);
  SkRect offset_rect = SkRect::MakeXYWH(2, 1, 10, 10);
  display_item_list->push<DrawRectOp>(offset_rect, raster_flags);
  display_item_list->EndPaintOfUnpaired(gfx::Rect(output_size));
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, output_size);
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_record_shader_tiled.png"));
}

TEST_F(OopPixelTest, DrawImageWithTargetColorSpace) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());
  options.target_color_params.color_space =
      gfx::ColorSpace::CreateDisplayP3D65();

  auto actual = Raster(display_item_list, options);

#if BUILDFLAG(IS_ANDROID)
  // Android has slight differences in color.
  FuzzyPixelOffByOneComparator comparator(/*discard_alpha=*/false);
#else
  ExactPixelComparator comparator(/*discard_alpha=*/false);
#endif

  ExpectEquals(actual, FILE_PATH_LITERAL("oop_image_target_color_space.png"),
               comparator);

  // Verify some conversion occurred here and that actual != bitmap.
  EXPECT_NE(actual.getColor(0, 0), SkColors::kMagenta.toSkColor());
}

TEST_F(OopPixelTest, DrawHdrImageWithMetadata) {
  constexpr gfx::Size size(100, 100);
  constexpr gfx::Rect rect(size);
  float sdr_luminance = 250.f;
  const float kPQMaxLuminance = 10000.f;

  const skcms_TransferFunction pq = SkNamedTransferFn::kPQ;
  skcms_TransferFunction pq_inv;
  skcms_TransferFunction_invert(&pq, &pq_inv);

  const float image_luminance = 1000.f;
  const float image_pq_pixel =
      skcms_TransferFunction_eval(&pq_inv, image_luminance / kPQMaxLuminance);

  // Create `image` with pixel value `image_pq_pixel` and PQ color space.
  sk_sp<SkImage> image;
  {
    SkBitmap bitmap;
    bitmap.allocPixelsFlags(
        SkImageInfo::MakeN32Premul(size.width(), size.height(),
                                   SkColorSpace::MakeSRGB()),
        SkBitmap::kZeroPixels_AllocFlag);

    SkCanvas canvas(bitmap, SkSurfaceProps{});
    SkColor4f color{image_pq_pixel, image_pq_pixel, image_pq_pixel, 1.f};
    canvas.drawColor(color);

    image = SkImage::MakeFromBitmap(bitmap);
    image = image->reinterpretColorSpace(
        SkColorSpace::MakeRGB(pq, SkNamedGamut::kSRGB));
  }

  // Create a DisplayItemList drawing `image`.
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();
  RasterOptions options(rect.size());
  {
    options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();
    options.target_color_params.enable_tone_mapping = true;
    options.target_color_params.sdr_max_luminance_nits = sdr_luminance;
    options.target_color_params.hdr_metadata = gfx::HDRMetadata();
  }

  // The exact value that `image_luminance` is mapped to may change as tone
  // mapping is tweaked. When
  constexpr float kCutoff = 0.95f;

  // Draw using image HDR metadata indicating that `image_luminance` is the
  // maximum luminance. The result should map the image to solid white (up
  // to rounding error).
  {
    options.target_color_params.hdr_metadata->color_volume_metadata =
        gfx::ColorVolumeMetadata(SkNamedPrimariesExt::kSRGB, image_luminance,
                                 0.f);

    auto actual = Raster(display_item_list, options);
    auto color = actual.getColor4f(0, 0);
    EXPECT_GT(color.fR, kCutoff);
    EXPECT_GT(color.fG, kCutoff);
    EXPECT_GT(color.fB, kCutoff);
  }

  // Draw using image HDR metadata indicating that 10,000 nits is the maximum
  // luminance. The result should map the image to something darker than solid
  // white.
  {
    options.target_color_params.hdr_metadata->color_volume_metadata =
        gfx::ColorVolumeMetadata(SkNamedPrimariesExt::kSRGB, kPQMaxLuminance,
                                 0.f);

    auto actual = Raster(display_item_list, options);
    auto color = actual.getColor4f(0, 0);
    EXPECT_LT(color.fR, kCutoff);
    EXPECT_LT(color.fG, kCutoff);
    EXPECT_LT(color.fB, kCutoff);
  }
}

TEST_F(OopPixelTest, DrawImageWithSourceColorSpace) {
  constexpr gfx::Rect rect(100, 100);

  auto color_space = gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace();

  sk_sp<SkImage> image = MakeSkImage(rect.size(), color_space);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();
  EXPECT_EQ(paint_image.color_space(), color_space.get());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());

  auto actual = Raster(display_item_list, options);

#if BUILDFLAG(IS_ANDROID)
  // Android has slight differences in color.
  FuzzyPixelComparator comparator(
      /*discard_alpha=*/false,
      /*error_pixels_percentage_limit=*/100.0f,
      /*small_error_pixels_percentage_limit=*/0.0f,
      /*avg_abs_error_limit=*/1.2f,
      /*max_abs_error_limit=*/2,
      /*small_error_threshold=*/0);
#else
  ExactPixelComparator comparator(/*discard_alpha=*/false);
#endif

  ExpectEquals(actual,
               FILE_PATH_LITERAL("oop_draw_image_source_color_space.png"),
               comparator);
}

TEST_F(OopPixelTest, DrawImageWithSourceAndTargetColorSpace) {
  constexpr gfx::Rect rect(100, 100);

  auto color_space = gfx::ColorSpace::CreateXYZD50().ToSkColorSpace();

  sk_sp<SkImage> image = MakeSkImage(rect.size(), color_space);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();
  EXPECT_EQ(paint_image.color_space(), color_space.get());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());
  options.target_color_params.color_space =
      gfx::ColorSpace::CreateDisplayP3D65();

  auto actual = Raster(display_item_list, options);

#if BUILDFLAG(IS_ANDROID)
  // Android has slight differences in color.
  FuzzyPixelOffByOneComparator comparator(/*discard_alpha=*/false);
#else
  ExactPixelComparator comparator(/*discard_alpha=*/false);
#endif

  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_image_both_color_space.png"),
               comparator);
}

TEST_F(OopPixelTest, DrawImageWithSetMatrix) {
  constexpr gfx::Rect rect(100, 100);

  sk_sp<SkImage> image = MakeSkImage(rect.size());
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(kDefaultFilterQuality));
  display_item_list->push<SetMatrixOp>(SkM44::Scale(0.5f, 0.5f));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_image_matrix.png"));
}

namespace {
class TestMailboxBacking : public TextureBacking {
 public:
  explicit TestMailboxBacking(gpu::Mailbox mailbox, SkImageInfo info)
      : mailbox_(mailbox), info_(info) {}

  const SkImageInfo& GetSkImageInfo() override { return info_; }
  gpu::Mailbox GetMailbox() const override { return mailbox_; }
  sk_sp<SkImage> GetAcceleratedSkImage() override { return nullptr; }
  sk_sp<SkImage> GetSkImageViaReadback() override { return nullptr; }
  bool readPixels(const SkImageInfo& dstInfo,
                  void* dstPixels,
                  size_t dstRowBytes,
                  int srcX,
                  int srcY) override {
    return false;
  }
  void FlushPendingSkiaOps() override {}

 private:
  gpu::Mailbox mailbox_;
  SkImageInfo info_;
};
}  // namespace

TEST_F(OopPixelTest, DrawMailboxBackedImage) {
  RasterOptions options(gfx::Size(16, 16));
  options.image_provider_raster_mode = PlaybackImageProvider::RasterMode::kOop;
  SkImageInfo backing_info = SkImageInfo::MakeN32Premul(
      options.resource_size.width(), options.resource_size.height());

  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(backing_info);

  SkCanvas canvas(expected_bitmap, SkSurfaceProps{});
  canvas.drawColor(SkColors::kMagenta);
  SkPaint green;
  green.setColor(SkColors::kGreen);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox src_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  ri->OrderingBarrierCHROMIUM();

  UploadPixels(ri, src_mailbox, expected_bitmap.info(), expected_bitmap);

  auto src_paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(PaintImage::GetNextId())
          .set_texture_backing(sk_sp<TestMailboxBacking>(new TestMailboxBacking(
                                   src_mailbox, backing_info)),
                               PaintImage::GetNextContentId())
          .TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawImageOp>(src_paint_image, 0.f, 0.f);
  display_item_list->EndPaintOfUnpaired(gfx::Rect(options.resource_size));
  display_item_list->Finalize();

  auto actual_bitmap = Raster(display_item_list, options);
  ExpectEquals(actual_bitmap, expected_bitmap);
}

TEST_F(OopPixelTest, Preclear) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->Finalize();

  RasterOptions options;
  options.resource_size = rect.size();
  options.full_raster_rect = rect;
  options.playback_rect = rect;
  options.background_color = SkColors::kMagenta;
  options.preclear = true;
  options.preclear_color = SkColors::kGreen;

  auto actual = Raster(display_item_list, options);

  auto expected = MakeSolidColorBitmap(rect.size(), SkColors::kGreen);
  ExpectEquals(actual, expected);
}

TEST_P(OopClearPixelTest, ClearingOpaqueCorner) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  if (IsPartialRaster()) {
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SkColors::kGreen;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a two pixel border from texels 7-9 on the column and 6-8 on row,
    // ignoring the top row and left column.
    canvas.drawRect(SkRect::MakeXYWH(7, 1, 2, 7), green);
    canvas.drawRect(SkRect::MakeXYWH(1, 6, 8, 2), green);
  } else {
    // Expect a two pixel border from texels 7-9 on the column and 6-8 on row.
    canvas.drawRect(SkRect::MakeXYWH(7, 0, 2, 8), green);
    canvas.drawRect(SkRect::MakeXYWH(0, 6, 9, 2), green);
  }

  ExpectEquals(result, bitmap);
}

TEST_F(OopPixelTest, ClearingOpaqueCornerExactEdge) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile whose content rect exactly lines up with
  // the edge of the resource.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SkColors::kGreen;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a one pixel border on the bottom/right edge.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(9, 0, 1, 10), green);
  canvas.drawRect(SkRect::MakeXYWH(0, 9, 10, 1), green);

  ExpectEquals(oop_result, bitmap);
}

TEST_F(OopPixelTest, ClearingOpaqueCornerPartialRaster) {
  // Verify that clears do nothing on an opaque corner tile whose
  // partial raster rect doesn't intersect the edge of the content.

  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 3);
  options.background_color = SkColors::kGreen;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Verify this is internal.
  EXPECT_NE(options.playback_rect.right(), options.full_raster_rect.right());
  EXPECT_NE(options.playback_rect.bottom(), options.full_raster_rect.bottom());

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clearing here because the playback rect is internal.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap);
}

TEST_P(OopClearPixelTest, ClearingOpaqueLeftEdge) {
  // Verify that a tile that intersects the left edge of content
  // but not other edges only clears the left pixels.
  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  int arbitrary_y = 10;
  options.full_raster_rect = gfx::Rect(0, arbitrary_y, 3, 10);
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the right column of pixels here to force partial raster.
    // Additionally ignore the top and bottom rows of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x(),
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 2);
  } else {
    options.playback_rect = options.full_raster_rect;
  }

  options.background_color = SkColors::kGreen;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a one pixel column border on the first column, ignoring the first
    // and the last rows.
    canvas.drawRect(SkRect::MakeXYWH(0, 1, 1, 8), green);
  } else {
    // Expect a one pixel column border on the first column.
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 10), green);
  }

  ExpectEquals(result, bitmap);
}

TEST_P(OopClearPixelTest, ClearingOpaqueRightEdge) {
  // Verify that a tile that intersects the right edge of content
  // but not other edges only clears the right pixels.
  RasterOptions options;
  gfx::Point arbitrary_offset(30, 40);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(3, 10));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the left column of pixels here to force partial raster.
    // Additionally ignore the top and bottom rows of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 2);
  } else {
    options.playback_rect = options.full_raster_rect;
  }

  options.background_color = SkColors::kGreen;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a two pixel column border from texels 2-4, ignoring the first and
    // the last rows.
    canvas.drawRect(SkRect::MakeXYWH(2, 1, 2, 8), green);
  } else {
    // Expect a two pixel column border from texels 2-4.
    canvas.drawRect(SkRect::MakeXYWH(2, 0, 2, 10), green);
  }

  ExpectEquals(result, bitmap);
}

TEST_P(OopClearPixelTest, ClearingOpaqueTopEdge) {
  // Verify that a tile that intersects only the top edge of content
  // but not other edges only clears the top pixels.

  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  int arbitrary_x = 10;
  options.full_raster_rect = gfx::Rect(arbitrary_x, 0, 10, 5);
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the bottom row of pixels here to force partial raster.
    // Additionally ignore the left and right columns of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y(),
                                      options.full_raster_rect.width() - 2,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SkColors::kGreen;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);

  if (IsPartialRaster()) {
    // Expect a one pixel border on the top row, ignoring the first and the last
    // columns.
    canvas.drawRect(SkRect::MakeXYWH(1, 0, 8, 1), green);
  } else {
    // Expect a one pixel border on the top row.
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 10, 1), green);
  }

  ExpectEquals(result, bitmap);
}

TEST_P(OopClearPixelTest, ClearingOpaqueBottomEdge) {
  // Verify that a tile that intersects the bottom edge of content
  // but not other edges only clears the bottom pixels.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(10, 5));
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom());
  if (IsPartialRaster()) {
    // Ignore the top row of pixels here to force partial raster.
    // Additionally ignore the left and right columns of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 2,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SkColors::kGreen;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);

  if (IsPartialRaster()) {
    // Expect a two pixel border from texels 4-6 on the row, ignoring the first
    // and the last columns.
    canvas.drawRect(SkRect::MakeXYWH(1, 4, 8, 2), green);
  } else {
    // Expect a two pixel border from texels 4-6 on the row
    canvas.drawRect(SkRect::MakeXYWH(0, 4, 10, 2), green);
  }

  ExpectEquals(result, bitmap);
}

TEST_F(OopPixelTest, ClearingOpaqueInternal) {
  // Verify that an internal opaque tile does no clearing.

  RasterOptions options;
  gfx::Point arbitrary_offset(35, 12);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  // Very large content rect to make this an internal tile.
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SkColors::kGreen;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  float arbitrary_scale = 1.2345f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clears here, as this tile does not intersect the edge of the
  // tile.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap);
}

TEST_F(OopPixelTest, ClearingTransparentCorner) {
  RasterOptions options;
  gfx::Point arbitrary_offset(5, 8);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SkColors::kTransparent;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SkColors::kTransparent);

  ExpectEquals(oop_result, bitmap);
}

TEST_F(OopPixelTest, ClearingTransparentInternalTile) {
  // Content rect much larger than full raster rect or playback rect.
  // This should still clear the tile.
  RasterOptions options;
  gfx::Point arbitrary_offset(100, 200);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SkColors::kTransparent;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Note that clearing of the tile should supersede any early outs due to an
  // empty display list. This is due to the fact that partial raster may in fact
  // result in no items being generated, in which case a clear should still
  // happen. See crbug.com/901897.
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();

  auto oop_result = Raster(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SkColors::kTransparent);

  ExpectEquals(oop_result, bitmap);
}

TEST_F(OopPixelTest, ClearingTransparentCornerPartialRaster) {
  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 4);
  options.background_color = SkColors::kTransparent;
  float arbitrary_scale = 0.23f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SkColors::kRed;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Result should be a red background with a cleared hole where the
  // playback_rect is.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  canvas.translate(-arbitrary_offset.x(), -arbitrary_offset.y());
  canvas.clipRect(gfx::RectToSkRect(options.playback_rect));
  canvas.drawColor(SkColors::kTransparent, SkBlendMode::kSrc);

  ExpectEquals(oop_result, bitmap);
}

// Test bitmap and playback rects in the raster options.
TEST_F(OopPixelTest, DrawRectPlaybackRect) {
  PaintFlags flags;
  flags.setColor(SkColorSetARGB(255, 250, 10, 20));
  gfx::Rect draw_rect(3, 1, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  RasterOptions options;
  options.full_raster_rect = gfx::Rect(1, 2, 10, 10);
  options.resource_size = options.full_raster_rect.size();
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = gfx::Rect(4, 2, 5, 6);
  options.background_color = SkColors::kMagenta;

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_rect_playback_rect.png"));
}

TEST_F(OopPixelTest, DrawRectScaleTransformOptions) {
  PaintFlags flags;
  // Use powers of two here to make floating point blending consistent.
  flags.setColor(SkColorSetARGB(128, 64, 128, 32));
  flags.setAntiAlias(true);
  gfx::Rect draw_rect(3, 4, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  // Draw a greenish transparent box, partially offset and clipped in the
  // bottom right.  It should appear near the upper left of a cyan background,
  // with the left and top sides of the greenish box partially blended due to
  // the post translate.
  RasterOptions options;
  options.resource_size = {20, 20};
  options.content_size = {25, 25};
  options.full_raster_rect = {5, 5, 20, 20};
  options.playback_rect = {5, 5, 13, 9};
  options.background_color = SkColors::kCyan;
  options.post_translate = {0.5f, 0.25f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_rect_scale_transform.png"));
}

TEST_F(OopPixelTest, DrawRectTransformOptionsFullRaster) {
  PaintFlags flags;
  // Use powers of two here to make floating point blending consistent.
  flags.setColor(SkColorSetRGB(64, 128, 32));
  flags.setAntiAlias(true);
  gfx::Rect draw_rect(0, 0, 19, 19);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  // The opaque rect above is 1px smaller than the canvas. With the subpixel
  // translation, the rect fills the whole canvas, but the pixels at the edges
  // are translucent. We should clear the canvas before drawing the rect, so
  // the translucent pixels at the edges should not expose the preclear color,
  // even if requires_clear is not true.
  RasterOptions options;
  options.resource_size = {20, 20};
  options.content_size = {25, 25};
  options.full_raster_rect = {5, 5, 20, 20};
  options.playback_rect = {5, 5, 20, 20};
  options.preclear = true;
  options.preclear_color = SkColors::kRed;
  options.post_translate = {0.5f, 0.25f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = MakeSolidColorBitmap(
      options.resource_size,
      SkColor4f::FromColor(SkColorSetARGB(255, 64, 128, 32)));

  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectQueryMiddleOfDisplayList) {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  std::vector<SkColor> colors = {
      SkColorSetARGB(255, 0, 0, 255),    SkColorSetARGB(255, 0, 255, 0),
      SkColorSetARGB(255, 0, 255, 255),  SkColorSetARGB(255, 255, 0, 0),
      SkColorSetARGB(255, 255, 0, 255),  SkColorSetARGB(255, 255, 255, 0),
      SkColorSetARGB(255, 255, 255, 255)};

  for (int i = 0; i < 20; ++i) {
    gfx::Rect draw_rect(0, i, 1, 1);
    PaintFlags flags;
    flags.setColor(colors[i % colors.size()]);
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
    display_item_list->EndPaintOfUnpaired(draw_rect);
  }
  display_item_list->Finalize();

  // Draw a "tile" in the middle of the display list with a post scale.
  RasterOptions options;
  options.resource_size = {10, 10};
  options.content_size = {20, 20};
  options.full_raster_rect = {0, 10, 1, 10};
  options.playback_rect = {0, 10, 1, 10};
  options.background_color = SkColors::kGray;
  options.post_translate = {0.f, 0.f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, FILE_PATH_LITERAL("oop_draw_rect_query.png"),
               FuzzyPixelOffByOneComparator(/*discard_alpha=*/false));
}

TEST_F(OopPixelTest, DrawRectColorSpace) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.target_color_params.color_space =
      gfx::ColorSpace::CreateDisplayP3D65();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColors::kGreen);
  display_item_list->push<DrawRectOp>(
      gfx::RectToSkRect(gfx::Rect(options.resource_size)), flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  SkBitmap expected = MakeSolidColorBitmap(
      options.resource_size,
      SkColor4f::FromColor(SkColorSetARGB(255, 117, 251, 76)));

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected);
}

sk_sp<SkTextBlob> BuildTextBlob(
    sk_sp<SkTypeface> typeface = SkTypeface::MakeDefault(),
    bool use_lcd_text = false) {
  if (!typeface) {
    typeface = SkTypeface::MakeFromName("monospace", SkFontStyle());
  }

  SkFont font;
  font.setTypeface(typeface);
  font.setHinting(SkFontHinting::kNormal);
  font.setSize(8.f);
  font.setBaselineSnap(false);
  font.setLinearMetrics(true);
  if (use_lcd_text) {
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  }

  return SkTextBlob::MakeFromString("Hamburgefons", font);
}

// A reasonable Y offset given the font parameters of BuildTextBlob() that
// ensures the text is not just drawn above the top edge of the surface.
static constexpr SkScalar kTextBlobY = 16.f;

// OopTextBlobPixelTest's test suite runs through the cross product of these
// strategies.
enum class TextBlobStrategy {
  kDirect,        // DrawTextBlobOp directly in the display list
  kDrawRecord,    // DrawRecordOp where the paint record includes text
  kRecordShader,  // DrawRectOp where the paint has a RecordShader with text
  kRecordFilter   // DrawRectOp where the paint has a RecordFilter with text
};
enum class FilterStrategy {
  kNone,        // No additional PaintFilter interacting with text
  kPaintFlags,  // A blur is added to the PaintFlags of the draw
  kSaveLayer    // An explicit save layer with blur is made before the draw
};
enum class MatrixStrategy {
  kIdentity,     // Identity matrix (no extra scale factor for text then)
  kScaled,       // Matrix is an axis-aligned scale factor
  kComplex,      // Matrix is not axis-aligned and scale must be decomposed
  kPerspective,  // Matrix has perspective and an approximate scale is needed
};
enum class LCDStrategy { kNo, kYes };

using TextBlobTestConfig = ::testing::
    tuple<TextBlobStrategy, FilterStrategy, MatrixStrategy, LCDStrategy>;

class OopTextBlobPixelTest
    : public OopPixelTest,
      public ::testing::WithParamInterface<TextBlobTestConfig> {
 public:
  void RunTest() {
    RasterOptions options;
    options.resource_size = gfx::Size(100, 100);
    options.content_size = options.resource_size;
    options.full_raster_rect = gfx::Rect(options.content_size);
    options.playback_rect = options.full_raster_rect;
    options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();
    options.use_lcd_text = UseLcdText();

    auto display_item_list = base::MakeRefCounted<DisplayItemList>();
    display_item_list->StartPaint();

    // Set matrix before any image filter is applied, which may force the
    // matrix to be decomposed into a transform compatible with the filter.
    display_item_list->push<ConcatOp>(GetMatrix());

    const bool save_layer =
        GetFilterStrategy(GetParam()) == FilterStrategy::kSaveLayer;
    sk_sp<PaintFilter> filter = MakeFilter();
    if (save_layer) {
      PaintFlags layer_flags;
      layer_flags.setImageFilter(std::move(filter));
      filter = nullptr;
      display_item_list->push<SaveLayerOp>(nullptr, &layer_flags);
    }

    PushDrawOp(display_item_list, std::move(filter));

    if (save_layer) {
      display_item_list->push<RestoreOp>();
    }

    display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
    display_item_list->Finalize();

    auto actual = Raster(display_item_list, options);
    auto expected = GetExpected(options.resource_size);

    // Drawing text into an image and then transforming that can lead to small
    // flakiness in devices, although in practice they are very imperceptible,
    // and distinctly different from using the wrong glyph or text params.
    float error_pixels_percentage = 0.f;
    int max_abs_error = 0;
#if BUILDFLAG(IS_ANDROID)
    // The nexus5 and nexus5x bots are particularly susceptible to small changes
    // when bilerping an image (not visible).
    const int sdk = base::android::BuildInfo::GetInstance()->sdk_int();
    if (sdk <= base::android::SDK_VERSION_MARSHMALLOW) {
      error_pixels_percentage = 10.f;
      max_abs_error = 20;
    } else {
      // Newer OSes occasionally have smaller flakes when using the real GPU
      error_pixels_percentage = 1.5f;
      max_abs_error = 2;
    }
#endif
    // Many platforms need very small tolerances under complex transforms,
    // and higher tolerances for perspective, since it triggers path rendering
    // for each glyph. Additionally, record filters require higher tolerance
    // because oop-r converts raster-at-scale to fixed-scale.
    float avg_error = max_abs_error;

    if (GetMatrixStrategy(GetParam()) == MatrixStrategy::kComplex) {
      const bool is_record_filter =
          GetTextBlobStrategy(GetParam()) == TextBlobStrategy::kRecordFilter;
      error_pixels_percentage =
          std::max(is_record_filter ? 12.f : 0.2f, error_pixels_percentage);
      max_abs_error = std::max(is_record_filter ? 220 : 2, max_abs_error);
      avg_error = std::max(is_record_filter ? 50.f : 2.f, avg_error);
    } else if (GetMatrixStrategy(GetParam()) == MatrixStrategy::kPerspective) {
      switch (GetTextBlobStrategy(GetParam())) {
        case TextBlobStrategy::kRecordFilter:
          error_pixels_percentage = std::max(13.f, error_pixels_percentage);
          max_abs_error = std::max(255, max_abs_error);
          avg_error = std::max(60.f, avg_error);
          break;
        case TextBlobStrategy::kRecordShader:
          // For kRecordShader+kPerspective the scale factor used to draw the
          // shader ends up being different for OOP-R vs using SkCanvas
          // directly. This causes some larger pixel differences as text spacing
          // subtly varies between `expected` and `actual`.
          error_pixels_percentage = std::max(19.0f, error_pixels_percentage);
#if BUILDFLAG(IS_ANDROID)
          // For some reason the text spacing is less consistent on Android
          // causing larger average difference between pixels.
          max_abs_error = std::max(237, max_abs_error);
          avg_error = std::max(60.9f, avg_error);
#else
          max_abs_error = std::max(228, max_abs_error);
          avg_error = std::max(40.2f, avg_error);
#endif
          break;
        default:
          error_pixels_percentage = std::max(4.0f, error_pixels_percentage);
          max_abs_error = std::max(36, max_abs_error);
          avg_error = std::max(36.0f, avg_error);
          break;
      }
    }

    FuzzyPixelComparator comparator(
        /*discard_alpha=*/false,
        /*error_pixels_percentage_limit=*/error_pixels_percentage,
        /*small_error_pixels_percentage_limit=*/0.0f,
        /*avg_abs_error_limit=*/avg_error,
        /*max_abs_error_limit=*/max_abs_error,
        /*small_error_threshold=*/0);
    ExpectEquals(actual, expected, comparator);
  }

  // Generates the expected image to compare against. This will draw on the GPU
  // thread and waits the current thread until results are ready.
  SkBitmap GetExpected(const gfx::Size& image_size) {
    SkBitmap bitmap;
    base::WaitableEvent waitable;
    auto* gpu_service = viz::TestGpuServiceHolder::GetInstance();

    // Draw the expected image to a GPU accelerated SkCanvas. This must be done
    // from the GPU thread so wait until that is done here.
    gpu_service->gpu_main_thread_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&OopTextBlobPixelTest::DrawExpectedOnGpuThread,
                       base::Unretained(this), image_size, std::ref(bitmap),
                       std::ref(waitable)));
    waitable.Wait();

    DCHECK(!bitmap.drawsNothing());
    return bitmap;
  }

  void DrawExpectedOnGpuThread(const gfx::Size& image_size,
                               SkBitmap& expected,
                               base::WaitableEvent& waitable) {
    auto* gpu_service = viz::TestGpuServiceHolder::GetInstance()->gpu_service();

    // Must make context current before drawing.
    auto context_state = gpu_service->GetContextState();
    ASSERT_TRUE(context_state->MakeCurrent(nullptr));

    gpu::raster::GrShaderCache::ScopedCacheUse cache_use(
        gpu_service->gr_shader_cache(), gpu::kDisplayCompositorClientId);

    // Setup a GPU accelerated SkSurface to draw to.
    SkImageInfo image_info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    SkSurfaceProps surface_props =
        UseLcdText() ? skia::LegacyDisplayGlobals::GetSkSurfaceProps(0)
                     : SkSurfaceProps(0, kUnknown_SkPixelGeometry);
    auto surface = SkSurface::MakeRenderTarget(
        context_state->gr_context(), SkBudgeted::kNo, image_info, 0,
        kTopLeft_GrSurfaceOrigin, &surface_props);

    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SkColors::kBlack);
    DrawExpectedToCanvas(*canvas);
    surface->flushAndSubmit();

    // Readback the expected image into `expected`.
    expected.allocPixels(image_info);
    bool success = surface->readPixels(expected, 0, 0);
    ASSERT_TRUE(success);

    waitable.Signal();
  }

  // Draws the expected image to SkCanvas directly.
  void DrawExpectedToCanvas(SkCanvas& canvas) {
    TextBlobTestConfig config = GetParam();

    // Set matrix before any image filter is applied, which may force the
    // matrix to be decomposed into a transform compatible with the filter.
    canvas.setMatrix(GetMatrix());

    TextBlobStrategy strategy = GetTextBlobStrategy(config);

    sk_sp<SkImageFilter> filter;
    if (GetFilterStrategy(config) != FilterStrategy::kNone) {
      filter =
          SkImageFilters::Blur(.1f, .1f, SkTileMode::kDecal, nullptr, nullptr);
    }

    const bool save_layer =
        GetFilterStrategy(config) == FilterStrategy::kSaveLayer;

    SkPaint save_paint;
    if (save_layer) {
      save_paint.setImageFilter(std::move(filter));
      filter = nullptr;
      canvas.saveLayer(nullptr, &save_paint);
    }

    SkPaint text_paint;
    text_paint.setColor(SkColors::kGreen);
    if (filter && (strategy == TextBlobStrategy::kDirect ||
                   strategy == TextBlobStrategy::kDrawRecord)) {
      text_paint.setImageFilter(std::move(filter));
      filter = nullptr;
    }

    auto text_blob = BuildTextBlob(SkTypeface::MakeDefault(), UseLcdText());

    if (strategy == TextBlobStrategy::kDirect) {
      // Draw text directly to the SkSurface.
      canvas.drawTextBlob(std::move(text_blob), 0, kTextBlobY, text_paint);
    } else {
      // All other strategies draw text to a SkPicture.
      SkPictureRecorder recorder;
      SkCanvas* record_canvas =
          recorder.beginRecording(SkRect::MakeWH(100, 100));
      record_canvas->drawTextBlob(std::move(text_blob), 0, kTextBlobY,
                                  text_paint);
      sk_sp<SkPicture> recording = recorder.finishRecordingAsPicture();

      if (strategy == TextBlobStrategy::kDrawRecord) {
        // Draw recorded SkPicture to SkSurface.
        canvas.drawPicture(recording.get());
      } else if (strategy == TextBlobStrategy::kRecordShader) {
        // Convert SkPicture to a shader and then draw the shader to SkSurface.
        SkRect shader_rect = SkRect::MakeWH(25, 25);
        auto draw_as_shader =
            recording->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                  SkFilterMode::kLinear, nullptr, &shader_rect);
        SkPaint shader_paint;
        shader_paint.setShader(std::move(draw_as_shader));
        if (filter) {
          shader_paint.setImageFilter(std::move(filter));
          filter = nullptr;
        }
        canvas.drawRect(SkRect::MakeWH(50, 50), shader_paint);
      } else {
        // Convert SkPicture to an image filter and then draw the filter to
        // SkSurface.
        DCHECK_EQ(strategy, TextBlobStrategy::kRecordFilter);
        sk_sp<SkImageFilter> draw_as_filter =
            SkImageFilters::Picture(std::move(recording));
        if (filter) {
          draw_as_filter = SkImageFilters::Compose(std::move(filter),
                                                   std::move(draw_as_filter));
          filter = nullptr;
        }
        SkPaint filter_paint;
        filter_paint.setImageFilter(std::move(draw_as_filter));
        canvas.drawRect(SkRect::MakeWH(50, 50), filter_paint);
      }
    }

    if (save_layer)
      canvas.restore();
  }

  sk_sp<PaintFilter> MakeFilter() {
    if (GetFilterStrategy(GetParam()) == FilterStrategy::kNone) {
      return nullptr;
    } else {
      // Keep the blur sigmas small to reduce test duration, it's the presence
      // of the blur filter that triggers the code path changes we care about.
      return sk_make_sp<BlurPaintFilter>(.1f, .1f, SkTileMode::kDecal, nullptr);
    }
  }

  SkM44 GetMatrix() {
    MatrixStrategy strategy = GetMatrixStrategy(GetParam());

    SkM44 m;  // Default constructed to identity
    if (strategy != MatrixStrategy::kIdentity) {
      // Scaled, Complex, and Perspective all have a 2x scale factor
      m.preScale(2.0f, 2.0f);
      if (strategy == MatrixStrategy::kComplex) {
        SkM44 skew = SkM44();
        skew.setRC(0, 1, 2.f);
        skew.setRC(1, 0, 2.f);
        m.preConcat(skew);
      } else if (strategy == MatrixStrategy::kPerspective) {
        SkM44 persp = SkM44::Perspective(0.01f, 10.f, SK_ScalarPI / 3.f);
        persp.preTranslate(0.f, 5.f, -0.1f);
        persp.preConcat(SkM44::Rotate({0.f, 1.f, 0.f}, 0.008f /* radians */));
        m.postConcat(persp);
      }
    }

    return m;
  }

  void PushDrawOp(scoped_refptr<DisplayItemList> display_list,
                  sk_sp<PaintFilter> filter) {
    TextBlobStrategy strategy = GetTextBlobStrategy(GetParam());

    auto text_blob = BuildTextBlob(SkTypeface::MakeDefault(), UseLcdText());

    PaintFlags text_flags;
    text_flags.setStyle(PaintFlags::kFill_Style);
    text_flags.setColor(SkColors::kGreen);
    if (filter && (strategy == TextBlobStrategy::kDirect ||
                   strategy == TextBlobStrategy::kDrawRecord)) {
      // If there's a filter, the only PaintFlags that are available for these
      // two text-drawing strategies is 'text_flags'.
      text_flags.setImageFilter(std::move(filter));
      filter = nullptr;
    }
    if (strategy == TextBlobStrategy::kDirect) {
      display_list->push<DrawTextBlobOp>(std::move(text_blob), 0.0f, kTextBlobY,
                                         text_flags);
      return;
    }

    // All remaining strategies add the DrawTextBlobOp to an inner paint record.
    auto paint_record = sk_make_sp<PaintOpBuffer>();
    paint_record->push<DrawTextBlobOp>(std::move(text_blob), 0.0f, kTextBlobY,
                                       text_flags);
    if (strategy == TextBlobStrategy::kDrawRecord) {
      display_list->push<DrawRecordOp>(std::move(paint_record));
      return;
    }

    PaintFlags record_flags;
    if (strategy == TextBlobStrategy::kRecordShader) {
      auto paint_record_shader = PaintShader::MakePaintRecord(
          paint_record, SkRect::MakeWH(25, 25), SkTileMode::kRepeat,
          SkTileMode::kRepeat, nullptr,
          PaintShader::ScalingBehavior::kRasterAtScale);
      // Force paint_flags to convert this to kFixedScale, so we can safely
      // compare pixels between direct and oop-r modes (since oop will convert
      // to kFixedScale no matter what.
      paint_record_shader->set_has_animated_images(true);

      record_flags.setShader(paint_record_shader);
      record_flags.setImageFilter(std::move(filter));
    } else {
      DCHECK(strategy == TextBlobStrategy::kRecordFilter);

      sk_sp<PaintFilter> paint_record_filter =
          sk_make_sp<RecordPaintFilter>(paint_record, SkRect::MakeWH(100, 100));
      // If there's an additional filter, we have to compose it with the
      // paint record filter.
      if (filter) {
        paint_record_filter = sk_make_sp<ComposePaintFilter>(
            std::move(filter), std::move(paint_record_filter));
      }
      record_flags.setImageFilter(std::move(paint_record_filter));
    }

    // Use bilerp sampling with the PaintRecord to help reduce max RGB error
    // from pixel-snapping flakiness when using NN sampling.
    record_flags.setFilterQuality(PaintFlags::FilterQuality::kLow);

    // The text blob is embedded in a paint record, which is attached to the
    // paint via a shader or image filter. Just draw a rect with the paint.
    display_list->push<DrawRectOp>(SkRect::MakeWH(50, 50), record_flags);
  }

  static TextBlobStrategy GetTextBlobStrategy(
      const TextBlobTestConfig& config) {
    return ::testing::get<0>(config);
  }
  static FilterStrategy GetFilterStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<1>(config);
  }
  static MatrixStrategy GetMatrixStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<2>(config);
  }
  static LCDStrategy GetLCDStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<3>(config);
  }

  bool UseLcdText() const {
    return GetLCDStrategy(GetParam()) == LCDStrategy::kYes;
  }

  static std::string PrintTestName(
      const ::testing::TestParamInfo<TextBlobTestConfig>& info) {
    std::stringstream ss;
    switch (GetTextBlobStrategy(info.param)) {
      case TextBlobStrategy::kDirect:
        ss << "Direct";
        break;
      case TextBlobStrategy::kDrawRecord:
        ss << "DrawRecord";
        break;
      case TextBlobStrategy::kRecordShader:
        ss << "RecordShader";
        break;
      case TextBlobStrategy::kRecordFilter:
        ss << "RecordFilter";
        break;
    }
    ss << "_";
    switch (GetFilterStrategy(info.param)) {
      case FilterStrategy::kNone:
        ss << "NoFilter";
        break;
      case FilterStrategy::kPaintFlags:
        ss << "FilterOnPaint";
        break;
      case FilterStrategy::kSaveLayer:
        ss << "FilterOnLayer";
        break;
    }
    ss << "_";
    switch (GetMatrixStrategy(info.param)) {
      case MatrixStrategy::kIdentity:
        ss << "IdentityCTM";
        break;
      case MatrixStrategy::kScaled:
        ss << "ScaledCTM";
        break;
      case MatrixStrategy::kComplex:
        ss << "ComplexCTM";
        break;
      case MatrixStrategy::kPerspective:
        ss << "PerspectiveCTM";
        break;
    }
    ss << "_";
    switch (GetLCDStrategy(info.param)) {
      case LCDStrategy::kNo:
        ss << "NoLCD";
        break;
      case LCDStrategy::kYes:
        ss << "LCD";
        break;
    }

    return ss.str();
  }
};

TEST_P(OopTextBlobPixelTest, Config) {
  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    P,
    OopTextBlobPixelTest,
    ::testing::Combine(::testing::Values(TextBlobStrategy::kDirect,
                                         TextBlobStrategy::kDrawRecord,
                                         TextBlobStrategy::kRecordShader,
                                         TextBlobStrategy::kRecordFilter),
                       ::testing::Values(FilterStrategy::kNone,
                                         FilterStrategy::kPaintFlags,
                                         FilterStrategy::kSaveLayer),
                       ::testing::Values(MatrixStrategy::kIdentity,
                                         MatrixStrategy::kScaled,
                                         MatrixStrategy::kComplex,
                                         MatrixStrategy::kPerspective),
                       ::testing::Values(LCDStrategy::kNo, LCDStrategy::kYes)),
    OopTextBlobPixelTest::PrintTestName);

void ClearFontCache(CompletionEvent* event) {
  SkGraphics::PurgeFontCache();
  event->Signal();
}

TEST_F(OopPixelTest, DrawTextMultipleRasterCHROMIUM) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();

  auto sk_typeface_1 = SkTypeface::MakeFromName("monospace", SkFontStyle());
  auto sk_typeface_2 = SkTypeface::MakeFromName("roboto", SkFontStyle());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColors::kGreen);
  display_item_list->push<DrawTextBlobOp>(BuildTextBlob(sk_typeface_1), 0.0f,
                                          kTextBlobY, flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  // Create another list with a different typeface.
  auto display_item_list_2 = base::MakeRefCounted<DisplayItemList>();
  display_item_list_2->StartPaint();
  display_item_list_2->push<DrawTextBlobOp>(BuildTextBlob(sk_typeface_2), 0.0f,
                                            kTextBlobY, flags);
  display_item_list_2->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list_2->Finalize();

  // Raster both these lists with 2 RasterCHROMIUM commands between a single
  // Begin/EndRaster sequence.
  options.additional_lists = {display_item_list_2};
  Raster(display_item_list, options);

  // Clear skia's font cache. No entries should remain since the service
  // should unpin everything.
  EXPECT_GT(SkGraphics::GetFontCacheUsed(), 0u);
  CompletionEvent event;
  raster_context_provider_->ExecuteOnGpuThread(
      base::BindOnce(&ClearFontCache, &event));
  event.Wait();
  EXPECT_EQ(SkGraphics::GetFontCacheUsed(), 0u);
}

TEST_F(OopPixelTest, DrawTextBlobPersistentShaderCache) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColors::kGreen);
  display_item_list->push<DrawTextBlobOp>(BuildTextBlob(), 0.0f, kTextBlobY,
                                          flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, options);

  // Perform the same operations on a software SkCanvas to produce an expected
  // bitmap.
  SkBitmap expected =
      MakeSolidColorBitmap(options.resource_size, SkColors::kBlack);
  SkCanvas canvas(expected, SkSurfaceProps{});
  canvas.drawColor(SkColors::kBlack);
  SkPaint paint;
  paint.setColor(SkColors::kGreen);
  canvas.drawTextBlob(BuildTextBlob(), 0, kTextBlobY, paint);

  // Allow 1% of pixels to be off by 1 due to differences between software and
  // GPU canvas.
  FuzzyPixelComparator comparator(
      /*discard_alpha=*/false,
      /*error_pixels_percentage_limit=*/1.0f,
      /*small_error_pixels_percentage_limit=*/0.0f,
      /*avg_abs_error_limit=*/1.0f,
      /*max_abs_error_limit=*/1,
      /*small_error_threshold=*/0);

  ExpectEquals(actual, expected, comparator);

  // Re-create the context so we start with an uninitialized skia memory cache
  // and use shaders from the persistent cache.
  InitializeOOPContext();
  actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected, comparator);
}

TEST_F(OopPixelTest, WritePixels) {
  gfx::Size dest_size(10, 10);
  RasterOptions options(dest_size);
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  std::vector<SkPMColor> expected_pixels(dest_size.width() * dest_size.height(),
                                         SkPreMultiplyARGB(255, 0, 0, 255));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(dest_size.width(), dest_size.height()),
      expected_pixels.data(), dest_size.width() * sizeof(SkColor));

  UploadPixels(ri, dest_mailbox, expected.info(), expected);

  SkBitmap actual = ReadbackMailbox(ri, dest_mailbox, options.resource_size);
  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  sii->DestroySharedImage(sync_token, dest_mailbox);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, CopySubTexture) {
  const gfx::Size size(16, 16);
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  const gfx::ColorSpace source_color_space = gfx::ColorSpace::CreateSRGB();
  const gfx::ColorSpace dest_color_space =
      gfx::ColorSpace::CreateDisplayP3D65();

  // Create data to upload in sRGB (solid green).
  SkBitmap upload_bitmap;
  {
    upload_bitmap.allocPixels(SkImageInfo::MakeN32Premul(
        size.width(), size.height(), source_color_space.ToSkColorSpace()));
    SkCanvas canvas(upload_bitmap, SkSurfaceProps{});
    SkPaint paint;
    paint.setColor(SkColors::kGreen);
    canvas.drawRect(SkRect::MakeWH(size.width(), size.height()), paint);
  }

  // Create an sRGB SharedImage and upload to it.
  gpu::Mailbox source_mailbox;
  {
    RasterOptions options(size);
    options.target_color_params.color_space = source_color_space;
    source_mailbox = CreateMailboxSharedImage(ri, sii, options,
                                              viz::ResourceFormat::RGBA_8888);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    ri->WritePixels(source_mailbox, 0, 0, GL_TEXTURE_2D,
                    upload_bitmap.rowBytes(), upload_bitmap.info(),
                    upload_bitmap.getPixels());
  }

  // Create a DisplayP3 SharedImage and copy to it.
  gpu::Mailbox dest_mailbox;
  {
    RasterOptions options(size);
    options.target_color_params.color_space = dest_color_space;
    dest_mailbox = CreateMailboxSharedImage(ri, sii, options,
                                            viz::ResourceFormat::RGBA_8888);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    ri->CopySubTexture(source_mailbox, dest_mailbox, GL_TEXTURE_2D, 0, 0, 0, 0,
                       size.width(), size.height(),
                       /*unpack_flip_y=*/GL_FALSE,
                       /*unpack_premultiply_alpha=*/GL_FALSE);
  }

  // Read the data back as DisplayP3, from the Display P3 SharedImage.
  SkBitmap readback_bitmap;
  {
    readback_bitmap.allocPixels(SkImageInfo::MakeN32Premul(
        size.width(), size.height(), dest_color_space.ToSkColorSpace()));

    ri->ReadbackImagePixels(dest_mailbox, readback_bitmap.info(),
                            readback_bitmap.rowBytes(), 0, 0,
                            readback_bitmap.getPixels());
  }

  // The pixel value should be unchanged, even though the source and dest are
  // in different color spaces. No color conversion (which would change the
  // pixel value) should have happened.
  EXPECT_EQ(*upload_bitmap.getAddr32(0, 0), *readback_bitmap.getAddr32(0, 0));
}

// The Android emulator does not support RED_8 or RG_88 texture formats.
#if !BUILDFLAG(IS_ANDROID_EMULATOR)
using OopYUVToRGBConfig = ::testing::tuple<gfx::ColorSpace, bool>;

class OopYUVToRGBPixelTest
    : public OopPixelTest,
      public ::testing::WithParamInterface<OopYUVToRGBConfig> {
 public:
  bool TestColorSpaceConversion() const {
    return ::testing::get<1>(GetParam());
  }

  gfx::ColorSpace DestinationColorSpace() const {
    return ::testing::get<0>(GetParam());
  }
};

TEST_P(OopYUVToRGBPixelTest, ConvertYUVToRGB) {
  // The source color space for the YUV image. If color space conversion is
  // disabled, or if `dest_color_space` is invalid, then this will be ignored.
  const gfx::ColorSpace source_color_space(gfx::ColorSpace::PrimaryID::P3,
                                           gfx::ColorSpace::TransferID::SRGB);

  // The output SharedImage color space.
  const gfx::ColorSpace dest_color_space = DestinationColorSpace();

  RasterOptions options(gfx::Size(16, 16));
  RasterOptions uv_options(gfx::Size(options.resource_size.width() / 2,
                                     options.resource_size.height() / 2));
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();

  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888, dest_color_space);

  constexpr viz::ResourceFormat format = viz::ResourceFormat::RED_8;
  gpu::Mailbox yuv_mailboxes[3]{
      CreateMailboxSharedImage(ri, sii, options, format),
      CreateMailboxSharedImage(ri, sii, uv_options, format),
      CreateMailboxSharedImage(ri, sii, uv_options, format)};

  SkImageInfo y_info = SkImageInfo::Make(
      options.resource_size.width(), options.resource_size.height(),
      kGray_8_SkColorType, kPremul_SkAlphaType,
      options.target_color_params.color_space.ToSkColorSpace());

  SkImageInfo uv_info = SkImageInfo::Make(
      uv_options.resource_size.width(), uv_options.resource_size.height(),
      kGray_8_SkColorType, kPremul_SkAlphaType,
      uv_options.target_color_params.color_space.ToSkColorSpace());

  // Create Y+U+V image planes for a solid blue image.
  SkBitmap y_bitmap;
  y_bitmap.allocPixels(y_info);
  memset(y_bitmap.getPixels(), 0x1d, y_bitmap.computeByteSize());

  SkBitmap u_bitmap;
  u_bitmap.allocPixels(uv_info);
  memset(u_bitmap.getPixels(), 0xff, u_bitmap.computeByteSize());

  SkBitmap v_bitmap;
  v_bitmap.allocPixels(uv_info);
  memset(v_bitmap.getPixels(), 0x6b, v_bitmap.computeByteSize());

  // Upload initial Y+U+V planes and convert to RGB.
  UploadPixels(ri, yuv_mailboxes[0], y_info, y_bitmap);
  UploadPixels(ri, yuv_mailboxes[1], uv_info, u_bitmap);
  UploadPixels(ri, yuv_mailboxes[2], uv_info, v_bitmap);

  ri->ConvertYUVAMailboxesToRGB(dest_mailbox, kJPEG_SkYUVColorSpace,
                                TestColorSpaceConversion()
                                    ? source_color_space.ToSkColorSpace().get()
                                    : nullptr,
                                SkYUVAInfo::PlaneConfig::kY_U_V,
                                SkYUVAInfo::Subsampling::k420, yuv_mailboxes);
  ri->OrderingBarrierCHROMIUM();
  SkBitmap actual_bitmap =
      ReadbackMailbox(ri, dest_mailbox, options.resource_size,
                      dest_color_space.ToSkColorSpace());

  SkColor expected_color =
      (TestColorSpaceConversion() && dest_color_space.IsValid())
          ? SkColorSetARGB(255, 61, 29, 252)
          : SkColorSetARGB(255, 0, 0, 254);
  SkBitmap expected_bitmap = MakeSolidColorBitmap(
      options.resource_size, SkColor4f::FromColor(expected_color));

  // Allow slight rounding error on all pixels.
  FuzzyPixelComparator comparator(
      /*discard_alpha=*/false,
      /*error_pixels_percentage_limit=*/100.0f,
      /*small_error_pixels_percentage_limit=*/0.0f,
      /*avg_abs_error_limit=*/2.f,
      /*max_abs_error_limit=*/2.f,
      /*small_error_threshold=*/0);
  ExpectEquals(actual_bitmap, expected_bitmap, comparator);

  gpu::SyncToken sync_token;
  sii->DestroySharedImage(sync_token, dest_mailbox);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[0]);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[1]);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[2]);
}

INSTANTIATE_TEST_SUITE_P(
    P,
    OopYUVToRGBPixelTest,
    ::testing::Combine(
        ::testing::Values(gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                          gfx::ColorSpace::TransferID::SRGB),
                          gfx::ColorSpace()),
        ::testing::Bool()));

TEST_F(OopPixelTest, ConvertNV12ToRGB) {
  RasterOptions options(gfx::Size(16, 16));
  RasterOptions uv_options(gfx::Size(options.resource_size.width() / 2,
                                     options.resource_size.height() / 2));
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();

  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  gpu::Mailbox y_uv_mailboxes[2]{
      CreateMailboxSharedImage(ri, sii, options, viz::ResourceFormat::RED_8),
      CreateMailboxSharedImage(ri, sii, uv_options, viz::ResourceFormat::RG_88),
  };

  SkImageInfo y_info = SkImageInfo::Make(
      options.resource_size.width(), options.resource_size.height(),
      kGray_8_SkColorType, kPremul_SkAlphaType,
      options.target_color_params.color_space.ToSkColorSpace());

  SkImageInfo uv_info = SkImageInfo::Make(
      uv_options.resource_size.width(), uv_options.resource_size.height(),
      kR8G8_unorm_SkColorType, kPremul_SkAlphaType,
      uv_options.target_color_params.color_space.ToSkColorSpace());

  // Create Y+UV image planes for a solid blue image.
  SkBitmap y_bitmap;
  y_bitmap.allocPixels(y_info);
  memset(y_bitmap.getPixels(), 0x1d, y_bitmap.computeByteSize());

  SkBitmap uv_bitmap;
  uv_bitmap.allocPixels(uv_info);
  uint8_t* uv_pix = static_cast<uint8_t*>(uv_bitmap.getPixels());
  for (size_t i = 0; i < uv_bitmap.computeByteSize(); i += 2) {
    uv_pix[i] = 0xff;
    uv_pix[i + 1] = 0x6d;
  }

  // Upload initial Y+UV planes and convert to RGB.
  UploadPixels(ri, y_uv_mailboxes[0], y_info, y_bitmap);
  UploadPixels(ri, y_uv_mailboxes[1], uv_info, uv_bitmap);

  ri->ConvertYUVAMailboxesToRGB(dest_mailbox, kJPEG_SkYUVColorSpace,
                                SkColorSpace::MakeSRGB().get(),
                                SkYUVAInfo::PlaneConfig::kY_UV,
                                SkYUVAInfo::Subsampling::k420, y_uv_mailboxes);
  ri->OrderingBarrierCHROMIUM();
  SkBitmap actual_bitmap =
      ReadbackMailbox(ri, dest_mailbox, options.resource_size);

  SkBitmap expected_bitmap = MakeSolidColorBitmap(
      options.resource_size,
      SkColor4f::FromColor(SkColorSetARGB(255, 2, 0, 254)));

  ExpectEquals(actual_bitmap, expected_bitmap);

  gpu::SyncToken sync_token;
  sii->DestroySharedImage(sync_token, dest_mailbox);
  sii->DestroySharedImage(sync_token, y_uv_mailboxes[0]);
  sii->DestroySharedImage(sync_token, y_uv_mailboxes[1]);
}
#endif  // !BUILDFLAG(IS_ANDROID_EMULATOR)

class OopPathPixelTest : public OopPixelTest,
                         public ::testing::WithParamInterface<bool> {
 public:
  bool AllowInlining() const { return GetParam(); }
  void RunTest() {
    auto* ri = static_cast<gpu::raster::RasterImplementation*>(
        raster_context_provider_->RasterInterface());
    uint32_t max_inlined_entry_size =
        AllowInlining() ? std::numeric_limits<uint32_t>::max() : 0u;
    ri->set_max_inlined_entry_size_for_testing(max_inlined_entry_size);

    RasterOptions options;
    options.resource_size = gfx::Size(100, 100);
    options.content_size = options.resource_size;
    options.full_raster_rect = gfx::Rect(options.content_size);
    options.playback_rect = options.full_raster_rect;
    options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();

    auto display_item_list = base::MakeRefCounted<DisplayItemList>();
    display_item_list->StartPaint();
    display_item_list->push<DrawColorOp>(SkColors::kWhite, SkBlendMode::kSrc);
    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(SkColors::kGreen);
    SkPath path;
    path.addCircle(20, 20, 10);
    display_item_list->push<DrawPathOp>(path, flags);
    flags.setColor(SkColors::kBlue);
    display_item_list->push<DrawRectOp>(SkRect::MakeWH(10, 10), flags);
    display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
    display_item_list->Finalize();

    // Allow 8 pixels in 100x100 image to be different due to non-AA pixel
    // rounding (hence 255 for error limit).
    FuzzyPixelComparator comparator(
        /*discard_alpha=*/false,
        /*error_pixels_percentage_limit=*/0.08f,
        /*small_error_pixels_percentage_limit=*/0.0f,
        /*avg_abs_error_limit=*/255,
        /*max_abs_error_limit=*/255,
        /*small_error_threshold=*/0);
    auto actual = Raster(display_item_list, options);
    ExpectEquals(actual, FILE_PATH_LITERAL("oop_path.png"), comparator);
  }
};

TEST_P(OopPathPixelTest, Basic) {
  RunTest();
}

TEST_F(OopPixelTest, RecordShaderExceedsMaxTextureSize) {
  const int max_texture_size =
      raster_context_provider_->ContextCapabilities().max_texture_size;
  const SkRect rect = SkRect::MakeWH(max_texture_size + 10, 10);

  auto shader_record = sk_make_sp<PaintRecord>();
  shader_record->push<DrawColorOp>(SkColors::kWhite, SkBlendMode::kSrc);
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColors::kGreen);
  shader_record->push<DrawRectOp>(rect, flags);
  auto shader = PaintShader::MakePaintRecord(
      shader_record, rect, SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);

  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = gfx::Size(rect.width(), rect.height());
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.target_color_params.color_space = gfx::ColorSpace::CreateSRGB();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SkColors::kWhite, SkBlendMode::kSrc);
  flags.setShader(shader);
  display_item_list->push<DrawRectOp>(rect, flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual,
               FILE_PATH_LITERAL("oop_record_shader_max_texture_size.png"));
}

INSTANTIATE_TEST_SUITE_P(P, OopClearPixelTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(P, OopPathPixelTest, ::testing::Bool());

}  // namespace
}  // namespace cc

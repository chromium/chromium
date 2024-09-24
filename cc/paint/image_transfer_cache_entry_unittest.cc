// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/image_transfer_cache_entry.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "cc/paint/paint_op_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

namespace cc {
namespace {

constexpr SkYUVColorSpace kJpegYUVColorSpace =
    SkYUVColorSpace::kJPEG_SkYUVColorSpace;

void MarkTextureAsReleased(SkImages::ReleaseContext context) {
  auto* released = static_cast<bool*>(context);
  DCHECK(!*released);
  *released = true;
}

// Checks if all the pixels in the |subset| of |image| are |expected_color|.
bool CheckRectIsSolidColor(const sk_sp<SkImage>& image,
                           SkColor expected_color,
                           const SkIRect& subset) {
  DCHECK_GE(image->width(), 1);
  DCHECK_GE(image->height(), 1);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(image->width(), image->height()))
    return false;
  SkPixmap pixmap;
  if (!bitmap.peekPixels(&pixmap))
    return false;
  if (!image->readPixels(pixmap, 0 /* srcX */, 0 /* srcY */))
    return false;
  for (int y = subset.fTop; y < subset.fBottom; y++) {
    for (int x = subset.fLeft; x < subset.fRight; x++) {
      if (bitmap.getColor(x, y) != expected_color)
        return false;
    }
  }
  return true;
}

// Checks if all the pixels in |image| are |expected_color|.
bool CheckImageIsSolidColor(const sk_sp<SkImage>& image,
                            SkColor expected_color) {
  return CheckRectIsSolidColor(
      image, expected_color, SkIRect::MakeWH(image->width(), image->height()));
}

// TODO(crbug.com/40266937): Implement test with Skia Graphite backend.
class ImageTransferCacheEntryTest
    : public testing::TestWithParam<SkYUVAInfo::PlaneConfig> {
 public:
  void SetUp() override {
    // Initialize a GL GrContext for Skia.
    auto surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                      gfx::Size());
    ASSERT_TRUE(surface);
    share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
    gl_context_ = base::MakeRefCounted<gl::GLContextEGL>(share_group_.get());
    ASSERT_TRUE(gl_context_);
    ASSERT_TRUE(gl_context_->Initialize(surface.get(), gl::GLContextAttribs()));
    //  The surface will be stored by the gl::GLContext.
    ASSERT_TRUE(gl_context_->default_surface());
    ASSERT_TRUE(gl_context_->MakeCurrentDefault());
    sk_sp<GrGLInterface> gl_interface(
        gl::init::CreateGrGLInterface(*gl_context_->GetVersionInfo()));
    gr_context_ = GrDirectContexts::MakeGL(std::move(gl_interface));
    ASSERT_TRUE(gr_context_);
  }

  // Creates the textures for a 64x64 YUV 4:2:0 image where all the samples in
  // all planes are 255u. This corresponds to an RGB color of (255, 121, 255)
  // assuming the JPEG YUV-to-RGB conversion formulas. Returns a list of
  // SkImages backed by the textures. Note that the number of textures depends
  // on the format (obtained using GetParam()). |release_flags| is set to a list
  // of boolean flags initialized to false. Each flag corresponds to a plane (in
  // the same order as the returned SkImages). When the texture for that plane
  // is released by Skia, that flag will be set to true. Returns an empty vector
  // on failure.
  std::vector<sk_sp<SkImage>> CreateTestYUVImage(
      base::HeapArray<bool>& release_flags) {
    std::vector<sk_sp<SkImage>> plane_images;
    release_flags = base::HeapArray<bool>();
    if (GetParam() == SkYUVAInfo::PlaneConfig::kY_U_V ||
        GetParam() == SkYUVAInfo::PlaneConfig::kY_V_U) {
      release_flags = base::HeapArray<bool>::CopiedFrom(
          std::to_array<bool>({false, false, false}));
      plane_images = {CreateSolidPlane(gr_context(), 64, 64, GL_R8_EXT,
                                       SkColors::kWhite, &release_flags[0]),
                      CreateSolidPlane(gr_context(), 32, 32, GL_R8_EXT,
                                       SkColors::kWhite, &release_flags[1]),
                      CreateSolidPlane(gr_context(), 32, 32, GL_R8_EXT,
                                       SkColors::kWhite, &release_flags[2])};
    } else if (GetParam() == SkYUVAInfo::PlaneConfig::kY_UV) {
      release_flags = base::HeapArray<bool>::CopiedFrom(
          std::to_array<bool>({false, false}));
      plane_images = {CreateSolidPlane(gr_context(), 64, 64, GL_R8_EXT,
                                       SkColors::kWhite, &release_flags[0]),
                      CreateSolidPlane(gr_context(), 32, 32, GL_RG8_EXT,
                                       SkColors::kWhite, &release_flags[1])};
    } else {
      NOTREACHED();
    }
    if (!base::Contains(plane_images, nullptr)) {
      return plane_images;
    }
    return {};
  }

  void DeletePendingTextures() {
    DCHECK(gr_context_);
    for (const auto& texture : textures_to_free_) {
      if (texture.isValid())
        gr_context_->deleteBackendTexture(texture);
    }
    gr_context_->flushAndSubmit();
    textures_to_free_.clear();
  }

  void TearDown() override {
    DeletePendingTextures();
    gr_context_.reset();
    gl_context_.reset();
    share_group_.reset();
  }

  GrDirectContext* gr_context() const { return gr_context_.get(); }

 private:
  // Uploads a texture corresponding to a single plane in a YUV image. All the
  // samples in the plane are set to |color|. The texture is not owned by Skia:
  // when Skia doesn't need it anymore, MarkTextureAsReleased() will be called.
  sk_sp<SkImage> CreateSolidPlane(GrDirectContext* gr_context,
                                  int width,
                                  int height,
                                  GrGLenum texture_format,
                                  const SkColor4f& color,
                                  bool* released) {
    GrBackendTexture allocated_texture = gr_context->createBackendTexture(
        width, height, GrBackendFormats::MakeGL(texture_format, GL_TEXTURE_2D),
        color, skgpu::Mipmapped::kNo, GrRenderable::kNo);
    if (!allocated_texture.isValid())
      return nullptr;
    textures_to_free_.push_back(allocated_texture);
    GrGLTextureInfo allocated_texture_info;
    if (!GrBackendTextures::GetGLTextureInfo(allocated_texture,
                                             &allocated_texture_info)) {
      return nullptr;
    }
    DCHECK_EQ(width, allocated_texture.width());
    DCHECK_EQ(height, allocated_texture.height());
    DCHECK(!allocated_texture.hasMipmaps());
    DCHECK(allocated_texture_info.fTarget == GL_TEXTURE_2D);
    *released = false;
    return SkImages::BorrowTextureFrom(
        gr_context, allocated_texture, kTopLeft_GrSurfaceOrigin,
        texture_format == GL_RG8_EXT ? kR8G8_unorm_SkColorType
                                     : kAlpha_8_SkColorType,
        kOpaque_SkAlphaType, nullptr /* colorSpace */, MarkTextureAsReleased,
        released);
  }

  std::vector<GrBackendTexture> textures_to_free_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> gl_context_;
  sk_sp<GrDirectContext> gr_context_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
};

// Disabled on Linux MSan Tests due to consistent segfault; crbug.com/1404443.
#if defined(MEMORY_SANITIZER) && BUILDFLAG(IS_LINUX)
#define MAYBE_Deserialize DISABLED_Deserialize
#else
#define MAYBE_Deserialize Deserialize
#endif
TEST_P(ImageTransferCacheEntryTest, MAYBE_Deserialize) {
  // Create a client-side entry from YUV planes. Use a different stride than the
  // width to test that alignment works correctly.
  const int image_width = 12;
  const int image_height = 10;
  const size_t yuv_strides[] = {16, 8, 8};

  SkYUVAInfo yuva_info({image_width, image_height},
                       SkYUVAInfo::PlaneConfig::kY_U_V,
                       SkYUVAInfo::Subsampling::k420, kJpegYUVColorSpace);
  SkYUVAPixmapInfo yuva_pixmap_info(
      yuva_info, SkYUVAPixmapInfo::DataType::kUnorm8, yuv_strides);
  SkYUVAPixmaps yuva_pixmaps = SkYUVAPixmaps::Allocate(yuva_pixmap_info);

  // rgb (255, 121, 255) -> yuv (255, 255, 255)
  const SkIRect bottom_color_rect =
      SkIRect::MakeXYWH(0, image_height / 2, image_width, image_height / 2);
  ASSERT_TRUE(yuva_pixmaps.plane(0).erase(SkColors::kWhite));
  ASSERT_TRUE(yuva_pixmaps.plane(1).erase(SkColors::kWhite));
  ASSERT_TRUE(yuva_pixmaps.plane(2).erase(SkColors::kWhite));

  // rgb (178, 0, 225) -> yuv (0, 255, 255)
  const SkIRect top_color_rect = SkIRect::MakeWH(image_width, image_height / 2);
  ASSERT_TRUE(yuva_pixmaps.plane(0).erase(SkColors::kBlack, &top_color_rect));

  auto client_entry(std::make_unique<ClientImageTransferCacheEntry>(
      ClientImageTransferCacheEntry::Image(yuva_pixmaps.planes().data(),
                                           yuva_info,
                                           nullptr /* decoded color space*/),
      true /* needs_mips */, std::nullopt));
  uint32_t size = client_entry->SerializedSize();
  auto data = PaintOpWriter::AllocateAlignedBuffer<uint8_t>(size);
  ASSERT_TRUE(client_entry->Serialize(
      base::make_span(static_cast<uint8_t*>(data.get()), size)));

  // Create service-side entry from the client-side serialize info
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  ASSERT_TRUE(entry->Deserialize(
      gr_context(), /*graphite_recorder=*/nullptr,
      base::make_span(static_cast<uint8_t*>(data.get()), size)));
  ASSERT_TRUE(entry->is_yuv());

  // Check color of pixels
  ASSERT_TRUE(CheckRectIsSolidColor(entry->image(), SkColorSetRGB(178, 0, 225),
                                    top_color_rect));
  ASSERT_TRUE(CheckRectIsSolidColor(
      entry->image(), SkColorSetRGB(255, 121, 255), bottom_color_rect));

  client_entry.reset();
  entry.reset();
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedNoMipsAtCreation) {
  base::HeapArray<bool> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(release_flags);
  const size_t plane_images_size = plane_images.size();
  ASSERT_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(GetParam())),
            plane_images_size);
  ASSERT_EQ(release_flags.size(), plane_images_size);

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains. The |buffer_byte_size| is only used for
  // accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, SkYUVAInfo::Subsampling::k420,
      kJpegYUVColorSpace, 0u /* buffer_byte_size */, false /* needs_mips */));

  // We didn't request generating mipmap chains, so the textures we created
  // above should stay alive until after the cache entry is deleted.
  EXPECT_TRUE(std::none_of(release_flags.begin(), release_flags.end(),
                           [](bool released) { return released; }));
  entry.reset();
  EXPECT_TRUE(std::all_of(release_flags.begin(), release_flags.end(),
                          [](bool released) { return released; }));
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedMipsAtCreation) {
  base::HeapArray<bool> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(release_flags);
  const size_t plane_images_size = plane_images.size();
  ASSERT_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(GetParam())),
            plane_images_size);
  ASSERT_EQ(release_flags.size(), plane_images_size);

  // Create a service-side image cache entry backed by these planes and request
  // generating mipmap chains at creation time. The |buffer_byte_size| is only
  // used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, SkYUVAInfo::Subsampling::k420,
      kJpegYUVColorSpace, 0u /* buffer_byte_size */, true /* needs_mips */));

  // We requested generating mipmap chains at creation time, so the textures we
  // created above should be released by now.
  EXPECT_TRUE(std::all_of(release_flags.begin(), release_flags.end(),
                          [](bool released) { return released; }));
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedMipsAfterCreation) {
  base::HeapArray<bool> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(release_flags);
  const size_t plane_images_size = plane_images.size();
  ASSERT_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(GetParam())),
            plane_images_size);
  ASSERT_EQ(release_flags.size(), plane_images_size);

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains at creation time. The |buffer_byte_size|
  // is only used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, SkYUVAInfo::Subsampling::k420,
      kJpegYUVColorSpace, 0u /* buffer_byte_size */, false /* needs_mips */));

  // We didn't request generating mip chains, so the textures we created above
  // should stay alive for now.
  EXPECT_TRUE(std::none_of(release_flags.begin(), release_flags.end(),
                           [](bool released) { return released; }));

  // Now request generating the mip chains.
  entry->EnsureMips();

  // Now the original textures should have been released.
  EXPECT_TRUE(std::all_of(release_flags.begin(), release_flags.end(),
                          [](bool released) { return released; }));
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

std::string TestParamToString(
    const testing::TestParamInfo<SkYUVAInfo::PlaneConfig>& param_info) {
  switch (param_info.param) {
    case SkYUVAInfo::PlaneConfig::kY_U_V:
      return "Y_U_V";
    case SkYUVAInfo::PlaneConfig::kY_V_U:
      return "Y_V_U";
    case SkYUVAInfo::PlaneConfig::kY_UV:
      return "Y_UV";
    default:
      NOTREACHED();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImageTransferCacheEntryTest,
                         ::testing::Values(SkYUVAInfo::PlaneConfig::kY_U_V,
                                           SkYUVAInfo::PlaneConfig::kY_V_U,
                                           SkYUVAInfo::PlaneConfig::kY_UV),
                         TestParamToString);

TEST(ImageTransferCacheEntryTestNoYUV, CPUImageWithMips) {
  GrMockOptions options;
  auto gr_context = GrDirectContext::MakeMock(&options);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gr_context->maxTextureSize() + 1, 10));

  ClientImageTransferCacheEntry client_entry(
      ClientImageTransferCacheEntry::Image(&bitmap.pixmap()), true,
      std::nullopt);
  const uint32_t storage_size = client_entry.SerializedSize();
  auto storage = PaintOpWriter::AllocateAlignedBuffer<uint8_t>(storage_size);
  client_entry.Serialize(base::make_span(storage.get(), storage_size));

  ServiceImageTransferCacheEntry service_entry;
  service_entry.Deserialize(gr_context.get(),
                            /*graphite_recorder=*/nullptr,
                            base::make_span(storage.get(), storage_size));
  ASSERT_TRUE(service_entry.image());
  auto pre_mip_image = service_entry.image();
  EXPECT_FALSE(pre_mip_image->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());

  service_entry.EnsureMips();
  ASSERT_TRUE(service_entry.image());
  EXPECT_FALSE(service_entry.image()->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());
  EXPECT_EQ(pre_mip_image, service_entry.image());
}

TEST(ImageTransferCacheEntryTestNoYUV, CPUImageAddMipsLater) {
  GrMockOptions options;
  auto gr_context = GrDirectContext::MakeMock(&options);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gr_context->maxTextureSize() + 1, 10));
  ClientImageTransferCacheEntry client_entry(
      ClientImageTransferCacheEntry::Image(&bitmap.pixmap()), false,
      std::nullopt);
  const uint32_t storage_size = client_entry.SerializedSize();
  auto storage = PaintOpWriter::AllocateAlignedBuffer<uint8_t>(storage_size);
  client_entry.Serialize(base::make_span(storage.get(), storage_size));

  ServiceImageTransferCacheEntry service_entry;
  service_entry.Deserialize(gr_context.get(),
                            /*graphite_recorder=*/nullptr,
                            base::make_span(storage.get(), storage_size));
  ASSERT_TRUE(service_entry.image());
  auto pre_mip_image = service_entry.image();
  EXPECT_FALSE(pre_mip_image->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());

  service_entry.EnsureMips();
  ASSERT_TRUE(service_entry.image());
  EXPECT_FALSE(service_entry.image()->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());
  EXPECT_EQ(pre_mip_image, service_entry.image());
}



}  // namespace
}  // namespace cc

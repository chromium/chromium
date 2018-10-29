// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <memory>

#include "cc/paint/draw_image.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_tile_task_runner.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

class FakeDiscardableManager {
 public:
  void SetGLES2Interface(viz::TestGLES2Interface* gl) { gl_ = gl; }
  void Initialize(GLuint texture_id) {
    EXPECT_EQ(textures_.end(), textures_.find(texture_id));
    textures_[texture_id] = kHandleLockedStart;
    live_textures_count_++;
  }
  void Unlock(GLuint texture_id) {
    EXPECT_NE(textures_.end(), textures_.find(texture_id));
    ExpectLocked(texture_id);
    textures_[texture_id]--;
  }
  bool Lock(GLuint texture_id) {
    EnforceLimit();

    EXPECT_NE(textures_.end(), textures_.find(texture_id));
    if (textures_[texture_id] >= kHandleUnlocked) {
      textures_[texture_id]++;
      return true;
    }
    return false;
  }

  void DeleteTexture(GLuint texture_id) {
    if (textures_.end() == textures_.find(texture_id))
      return;

    ExpectLocked(texture_id);
    textures_[texture_id] = kHandleDeleted;
    live_textures_count_--;
  }

  void set_cached_textures_limit(size_t limit) {
    cached_textures_limit_ = limit;
  }

  void ExpectLocked(GLuint texture_id) {
    EXPECT_TRUE(textures_.end() != textures_.find(texture_id));

    // Any value > kHandleLockedStart represents a locked texture. As we
    // increment this value with each lock, we need the entire range and can't
    // add additional values > kHandleLockedStart in the future.
    EXPECT_GE(textures_[texture_id], kHandleLockedStart);
    EXPECT_LE(textures_[texture_id], kHandleLockedEnd);
  }

 private:
  void EnforceLimit() {
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
      if (live_textures_count_ <= cached_textures_limit_)
        return;
      if (it->second != kHandleUnlocked)
        continue;

      it->second = kHandleDeleted;
      gl_->TestGLES2Interface::DeleteTextures(1, &it->first);
      live_textures_count_--;
    }
  }

  const int32_t kHandleDeleted = 0;
  const int32_t kHandleUnlocked = 1;
  const int32_t kHandleLockedStart = 2;
  const int32_t kHandleLockedEnd = std::numeric_limits<int32_t>::max();

  std::map<GLuint, int32_t> textures_;
  size_t live_textures_count_ = 0;
  size_t cached_textures_limit_ = std::numeric_limits<size_t>::max();
  viz::TestGLES2Interface* gl_ = nullptr;
};

class FakeGPUImageDecodeTestGLES2Interface : public viz::TestGLES2Interface,
                                             public viz::TestContextSupport {
 public:
  explicit FakeGPUImageDecodeTestGLES2Interface(
      FakeDiscardableManager* discardable_manager,
      TransferCacheTestHelper* transfer_cache_helper)
      : extension_string_(
            "GL_EXT_texture_format_BGRA8888 GL_OES_rgb8_rgba8 "
            "GL_OES_texture_npot "
            "GL_OES_texture_half_float GL_OES_texture_half_float_linear"),
        discardable_manager_(discardable_manager),
        transfer_cache_helper_(transfer_cache_helper) {}

  ~FakeGPUImageDecodeTestGLES2Interface() override {
    // All textures / framebuffers / renderbuffers should be cleaned up.
    EXPECT_EQ(0u, NumTextures());
    EXPECT_EQ(0u, NumFramebuffers());
    EXPECT_EQ(0u, NumRenderbuffers());
  }

  void InitializeDiscardableTextureCHROMIUM(GLuint texture_id) override {
    discardable_manager_->Initialize(texture_id);
  }
  void UnlockDiscardableTextureCHROMIUM(GLuint texture_id) override {
    discardable_manager_->Unlock(texture_id);
  }
  bool LockDiscardableTextureCHROMIUM(GLuint texture_id) override {
    return discardable_manager_->Lock(texture_id);
  }

  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override {
    return discardable_manager_->Lock(texture_id);
  }
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override {}

  void* MapTransferCacheEntry(size_t serialized_size) override {
    mapped_entry_size_ = serialized_size;
    mapped_entry_.reset(new uint8_t[serialized_size]);
    return mapped_entry_.get();
  }

  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override {
    transfer_cache_helper_->CreateEntryDirect(
        MakeEntryKey(type, id),
        base::make_span(mapped_entry_.get(), mapped_entry_size_));
    mapped_entry_ = nullptr;
    mapped_entry_size_ = 0;
  }

  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override {
    return transfer_cache_helper_->LockEntryDirect(MakeEntryKey(type, id));
  }
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override {
    std::vector<std::pair<TransferCacheEntryType, uint32_t>> keys;
    keys.reserve(entries.size());
    for (const auto& e : entries)
      keys.emplace_back(MakeEntryKey(e.first, e.second));
    transfer_cache_helper_->UnlockEntriesDirect(keys);
  }
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override {
    transfer_cache_helper_->DeleteEntryDirect(MakeEntryKey(type, id));
  }

  std::pair<TransferCacheEntryType, uint32_t> MakeEntryKey(uint32_t type,
                                                           uint32_t id) {
    DCHECK_LE(type, static_cast<uint32_t>(TransferCacheEntryType::kLast));
    return std::make_pair(static_cast<TransferCacheEntryType>(type), id);
  }

  // viz::TestGLES2Interface:
  const GLubyte* GetString(GLenum name) override {
    switch (name) {
      case GL_EXTENSIONS:
        return reinterpret_cast<const GLubyte*>(extension_string_.c_str());
      case GL_VERSION:
        return reinterpret_cast<const GLubyte*>("4.0 Null GL");
      case GL_SHADING_LANGUAGE_VERSION:
        return reinterpret_cast<const GLubyte*>("4.20.8 Null GLSL");
      case GL_VENDOR:
        return reinterpret_cast<const GLubyte*>("Null Vendor");
      case GL_RENDERER:
        return reinterpret_cast<const GLubyte*>("The Null (Non-)Renderer");
    }
    return nullptr;
  }
  void GetIntegerv(GLenum name, GLint* params) override {
    switch (name) {
      case GL_MAX_TEXTURE_IMAGE_UNITS:
        *params = 8;
        return;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        *params = 8;
        return;
      case GL_MAX_RENDERBUFFER_SIZE:
        *params = 2048;
        return;
      case GL_MAX_VERTEX_ATTRIBS:
        *params = 8;
        return;
      default:
        break;
    }
    TestGLES2Interface::GetIntegerv(name, params);
  }
  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    for (GLsizei i = 0; i < n; i++) {
      discardable_manager_->DeleteTexture(textures[i]);
    }
    TestGLES2Interface::DeleteTextures(n, textures);
  }

 private:
  const std::string extension_string_;
  FakeDiscardableManager* discardable_manager_;
  TransferCacheTestHelper* transfer_cache_helper_;
  size_t mapped_entry_size_ = 0;
  std::unique_ptr<uint8_t[]> mapped_entry_;
};

class GPUImageDecodeTestMockContextProvider : public viz::TestContextProvider {
 public:
  static scoped_refptr<GPUImageDecodeTestMockContextProvider> Create(
      FakeDiscardableManager* discardable_manager,
      TransferCacheTestHelper* transfer_cache_helper) {
    return new GPUImageDecodeTestMockContextProvider(
        std::make_unique<FakeGPUImageDecodeTestGLES2Interface>(
            discardable_manager, transfer_cache_helper),
        std::make_unique<FakeGPUImageDecodeTestGLES2Interface>(
            discardable_manager, transfer_cache_helper));
  }

 private:
  ~GPUImageDecodeTestMockContextProvider() override = default;
  GPUImageDecodeTestMockContextProvider(
      std::unique_ptr<viz::TestContextSupport> support,
      std::unique_ptr<viz::TestGLES2Interface> gl)
      : TestContextProvider(std::move(support), std::move(gl), true) {}
};

SkMatrix CreateMatrix(const SkSize& scale, bool is_decomposable) {
  SkMatrix matrix;
  matrix.setScale(scale.width(), scale.height());

  if (!is_decomposable) {
    // Perspective is not decomposable, add it.
    matrix[SkMatrix::kMPersp0] = 0.1f;
  }

  return matrix;
}

size_t kGpuMemoryLimitBytes = 96 * 1024 * 1024;

class GpuImageDecodeCacheTest
    : public ::testing::TestWithParam<
          std::pair<SkColorType, bool /* use_transfer_cache */>> {
 public:
  void SetUp() override {
    context_provider_ = GPUImageDecodeTestMockContextProvider::Create(
        &discardable_manager_, &transfer_cache_helper_);
    discardable_manager_.SetGLES2Interface(
        context_provider_->UnboundTestContextGL());
    context_provider_->BindToCurrentThread();
    {
      viz::RasterContextProvider::ScopedRasterContextLock context_lock(
          context_provider_.get());
      transfer_cache_helper_.SetGrContext(context_provider_->GrContext());
      max_texture_size_ =
          context_provider_->ContextCapabilities().max_texture_size;
    }
    use_transfer_cache_ = GetParam().second;
    color_type_ = GetParam().first;
  }

  std::unique_ptr<GpuImageDecodeCache> CreateCache() {
    return std::make_unique<GpuImageDecodeCache>(
        context_provider_.get(), use_transfer_cache_, color_type_,
        kGpuMemoryLimitBytes, max_texture_size_,
        PaintImage::kDefaultGeneratorClientId);
  }

  PaintImage CreatePaintImageInternal(
      const gfx::Size& size,
      sk_sp<SkColorSpace> color_space = nullptr,
      PaintImage::Id id = PaintImage::kInvalidId) {
    const bool allocate_encoded_memory = true;
    return CreateDiscardablePaintImage(
        size, color_space, allocate_encoded_memory, id, color_type_);
  }

  PaintImage CreateBitmapImageInternal(const gfx::Size& size) {
    return CreateBitmapImage(size, color_type_);
  }

  gfx::ColorSpace DefaultColorSpace() {
    if (color_type_ != kRGBA_F16_SkColorType)
      return gfx::ColorSpace::CreateSRGB();
    return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTEST432_1,  // P3
                           gfx::ColorSpace::TransferID::LINEAR);
  }

  GPUImageDecodeTestMockContextProvider* context_provider() {
    return context_provider_.get();
  }

  void ExpectIfNotUsingTransferCache(bool value) {
    if (!use_transfer_cache_) {
      EXPECT_TRUE(value);
    }
  }

  void SetCachedTexturesLimit(size_t limit) {
    discardable_manager_.set_cached_textures_limit(limit);
    transfer_cache_helper_.SetCachedItemsLimit(limit);
  }

  // If this is an image-backed DecodedDrawImage, does nothing. Otherwise this
  // retreives the image from the transfer cache and builds a new
  // DecodedDrawImage.
  DecodedDrawImage EnsureImageBacked(DecodedDrawImage&& draw_image) {
    if (draw_image.transfer_cache_entry_id()) {
      EXPECT_TRUE(use_transfer_cache_);
      auto* image_entry =
          transfer_cache_helper_.GetEntryAs<ServiceImageTransferCacheEntry>(
              *draw_image.transfer_cache_entry_id());
      if (draw_image.transfer_cache_entry_needs_mips())
        image_entry->EnsureMips();
      DecodedDrawImage new_draw_image(
          image_entry->image(), draw_image.src_rect_offset(),
          draw_image.scale_adjustment(), draw_image.filter_quality(),
          draw_image.is_budgeted());
      return new_draw_image;
    }

    return draw_image;
  }

  sk_sp<SkImage> GetLastTransferredImage() {
    auto& key = transfer_cache_helper_.GetLastAddedEntry();
    ServiceTransferCacheEntry* entry =
        transfer_cache_helper_.GetEntryInternal(key.first, key.second);
    if (!entry)
      return nullptr;
    CHECK_EQ(TransferCacheEntryType::kImage, entry->Type());
    return static_cast<ServiceImageTransferCacheEntry*>(entry)->image();
  }

 protected:
  FakeDiscardableManager discardable_manager_;
  scoped_refptr<GPUImageDecodeTestMockContextProvider> context_provider_;
  TransferCacheTestHelper transfer_cache_helper_;
  bool use_transfer_cache_;
  SkColorType color_type_;
  int max_texture_size_ = 0;
};

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageSameImage) {
  auto cache = CreateCache();
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache->GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageSmallerScale) {
  auto cache = CreateCache();
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache->GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache->UnrefImage(draw_image);
  cache->UnrefImage(another_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLowerQuality) {
  auto cache = CreateCache();
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kHigh_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kLow_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache->GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache->UnrefImage(draw_image);
  cache->UnrefImage(another_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageDifferentImage) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  PaintImage second_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage second_draw_image(
      second_image,
      SkIRect::MakeWH(second_image.width(), second_image.height()), quality,
      CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(first_draw_image);
  cache->UnrefImage(second_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLargerScale) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());

  cache->UnrefImage(first_draw_image);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult third_result = cache->GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(second_draw_image);
  cache->UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLargerScaleNoReuse) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult third_result = cache->GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == first_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(first_draw_image);
  cache->UnrefImage(second_draw_image);
  cache->UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageHigherQuality) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      kLow_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());

  cache->UnrefImage(first_draw_image);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);

  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());
  cache->UnrefImage(second_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyDecodedAndLocked) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the decode but don't complete it (this will keep the decode locked).
  TestTileTaskRunner::ScheduleTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::RunTask(result.task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Get the image again - we should have an upload task, but no dependent
  // decode task, as the decode was already locked.
  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task);
  EXPECT_EQ(another_result.task->dependencies().size(), 0u);

  TestTileTaskRunner::ProcessTask(another_result.task.get());

  // Finally, complete the original decode task.
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());

  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyDecodedNotLocked) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the decode.
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Unref the image.
  cache->UnrefImage(draw_image);

  // Get the image again - we should have an upload task and a dependent decode
  // task - this dependent task will typically just re-lock the image.
  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task);
  EXPECT_EQ(another_result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(another_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(another_result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyUploaded) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageCanceledGetsNewTask) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Fully cancel everything (so the raster would unref things).
  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);

  // Here a new task is created.
  ImageDecodeCache::TaskResult third_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageCanceledWhileReffedGetsNewTask) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ASSERT_GT(result.task->dependencies().size(), 0u);
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // 2 Unrefs, so that the decode is unlocked as well.
  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  ImageDecodeCache::TaskResult third_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  ASSERT_GT(third_result.task->dependencies().size(), 0u);
  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  // Unref!
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageUploadCanceledButDecodeRun) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Unref the image.
  cache->UnrefImage(draw_image);

  // Run the decode task. Even though the image only has a decode ref at this
  // point, this should complete successfully.
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
}

TEST_P(GpuImageDecodeCacheTest, NoTaskForImageAlreadyFailedDecoding) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache->SetImageDecodingFailedForTesting(draw_image);

  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(another_result.need_unref);
  EXPECT_EQ(another_result.task.get(), nullptr);

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDraw) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetLargeDecodedImageForDraw) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  ExpectIfNotUsingTransferCache(
      cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawAtRasterDecode) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache->SetWorkingSetLimitsForTesting(0 /* max_bytes */, 0 /* max_items */);

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);

  // Our 0 working set size shouldn't prevent caching of unlocked discardable,
  // so our single entry should be cached.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 1u);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawLargerScale) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  DrawImage larger_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult larger_result = cache->GetTaskForImageAndRef(
      larger_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(larger_result.need_unref);
  EXPECT_TRUE(larger_result.task);

  TestTileTaskRunner::ProcessTask(larger_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(larger_result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage larger_decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(larger_draw_image));
  EXPECT_TRUE(larger_decoded_draw_image.image());
  EXPECT_TRUE(larger_decoded_draw_image.is_budgeted());
  EXPECT_TRUE(larger_decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  EXPECT_FALSE(decoded_draw_image.image() == larger_decoded_draw_image.image());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
  cache->DrawWithImageFinished(larger_draw_image, larger_decoded_draw_image);
  cache->UnrefImage(larger_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawHigherQuality) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable);

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  DrawImage higher_quality_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult hq_result = cache->GetTaskForImageAndRef(
      higher_quality_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(hq_result.need_unref);
  EXPECT_TRUE(hq_result.task);
  TestTileTaskRunner::ProcessTask(hq_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(hq_result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage larger_decoded_draw_image = EnsureImageBacked(
      cache->GetDecodedImageForDraw(higher_quality_draw_image));
  EXPECT_TRUE(larger_decoded_draw_image.image());
  EXPECT_TRUE(larger_decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  EXPECT_FALSE(decoded_draw_image.image() == larger_decoded_draw_image.image());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
  cache->DrawWithImageFinished(higher_quality_draw_image,
                               larger_decoded_draw_image);
  cache->UnrefImage(higher_quality_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawNegative) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(-0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  EXPECT_EQ(decoded_draw_image.image()->width(), 50);
  EXPECT_EQ(decoded_draw_image.image()->height(), 50);
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetLargeScaledDecodedImageForDraw) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 48000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  // The mip level scale should never go below 0 in any dimension.
  EXPECT_EQ(1, decoded_draw_image.image()->width());
  EXPECT_EQ(24000, decoded_draw_image.image()->height());

  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);

  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  ExpectIfNotUsingTransferCache(
      cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, AtRasterUsedDirectlyIfSpaceAllows) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache->SetWorkingSetLimitsForTesting(0 /* max_bytes */, 0 /* max_items */);

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);

  // Increase memory limit and attempt to use the same image. It should be in
  // available for ref.
  cache->SetWorkingSetLimitsForTesting(96 * 1024 * 1024 /* max_bytes */,
                                       256 /* max_items */);
  ImageDecodeCache::TaskResult another_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest,
       GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache->SetWorkingSetLimitsForTesting(0 /* max_bytes */, 0 /* max_items */);

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage another_decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_FALSE(another_decoded_draw_image.is_budgeted());
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->DrawWithImageFinished(draw_image, another_decoded_draw_image);

  // Our 0 working set size shouldn't prevent caching of unlocked discardable,
  // so our single entry should be cached.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 1u);
}

TEST_P(GpuImageDecodeCacheTest,
       GetLargeDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache->SetWorkingSetLimitsForTesting(0 /* max_bytes */, 0 /* max_items */);
  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  ExpectIfNotUsingTransferCache(
      cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage second_decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(second_decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.is_budgeted());
  EXPECT_FALSE(second_decoded_draw_image.image()->isTextureBacked());
  ExpectIfNotUsingTransferCache(
      cache->DiscardableIsLockedForTesting(draw_image));

  cache->DrawWithImageFinished(draw_image, second_decoded_draw_image);
  EXPECT_FALSE(cache->DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, ZeroSizedImagesAreSkipped) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.f, 0.f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_FALSE(decoded_draw_image.image());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NonOverlappingSrcRectImagesAreSkipped) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(150, 150, image.width(), image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_FALSE(decoded_draw_image.image());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, CanceledTasksDoNotCountAgainstBudget) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(0, 0, image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_NE(0u, cache->GetNumCacheEntriesForTesting());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::CancelTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache->UnrefImage(draw_image);
  EXPECT_EQ(0u, cache->GetWorkingSetBytesForTesting());
}

TEST_P(GpuImageDecodeCacheTest, ShouldAggressivelyFreeResources) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());

    cache->UnrefImage(draw_image);

    // We should now have data image in our cache.
    EXPECT_GT(cache->GetNumCacheEntriesForTesting(), 0u);

    // Tell our cache to aggressively free resources.
    cache->SetShouldAggressivelyFreeResources(true);
    EXPECT_EQ(0u, cache->GetNumCacheEntriesForTesting());
  }

  // Attempting to upload a new image should succeed, but the image should not
  // be cached past its use.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image);

    EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 0u);
  }

  // We now tell the cache to not aggressively free resources. The image may
  // now be cached past its use.
  cache->SetShouldAggressivelyFreeResources(false);
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image);

    EXPECT_GT(cache->GetNumCacheEntriesForTesting(), 0u);
  }
}

TEST_P(GpuImageDecodeCacheTest, OrphanedImagesFreeOnReachingZeroRefs) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create a downscaled image.
  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache->GetWorkingSetBytesForTesting(),
            cache->GetDrawImageSizeForTesting(first_draw_image));

  // Create a larger version of |first_image|, this should immediately free the
  // memory used by |first_image| for the smaller scale.
  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(second_draw_image);

  // Unref the first image, it was orphaned, so it should be immediately
  // deleted.
  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  cache->UnrefImage(first_draw_image);

  // The cache should have exactly one image.
  EXPECT_EQ(1u, cache->GetNumCacheEntriesForTesting());
  EXPECT_EQ(0u, cache->GetInUseCacheEntriesForTesting());
}

TEST_P(GpuImageDecodeCacheTest, OrphanedZeroRefImagesImmediatelyDeleted) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create a downscaled image.
  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  cache->UnrefImage(first_draw_image);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 1u);
  EXPECT_EQ(cache->GetInUseCacheEntriesForTesting(), 0u);

  // Create a larger version of |first_image|, this should immediately free the
  // memory used by |first_image| for the smaller scale.
  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(second_draw_image);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 1u);
  EXPECT_EQ(cache->GetInUseCacheEntriesForTesting(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, QualityCappedAtMedium) {
  auto cache = CreateCache();
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  // Create an image with kLow_FilterQuality.
  DrawImage low_draw_image(image,
                           SkIRect::MakeWH(image.width(), image.height()),
                           kLow_SkFilterQuality, matrix,
                           PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult low_result = cache->GetTaskForImageAndRef(
      low_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(low_result.need_unref);
  EXPECT_TRUE(low_result.task);

  // Get the same image at kMedium_SkFilterQuality. We can't re-use low, so we
  // should get a new task/ref.
  DrawImage medium_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kMedium_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult medium_result = cache->GetTaskForImageAndRef(
      medium_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(medium_result.need_unref);
  EXPECT_TRUE(medium_result.task.get());
  EXPECT_FALSE(low_result.task.get() == medium_result.task.get());

  // Get the same image at kHigh_FilterQuality. We should re-use medium.
  DrawImage large_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult large_result = cache->GetTaskForImageAndRef(
      large_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(large_result.need_unref);
  EXPECT_TRUE(medium_result.task.get() == large_result.task.get());

  TestTileTaskRunner::ProcessTask(low_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(low_result.task.get());
  TestTileTaskRunner::ProcessTask(medium_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(medium_result.task.get());

  cache->UnrefImage(low_draw_image);
  cache->UnrefImage(medium_draw_image);
  cache->UnrefImage(large_draw_image);
}

// Ensure that switching to a mipped version of an image after the initial
// cache entry creation doesn't cause a buffer overflow/crash.
TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawMipUsageChange) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create an image decode task and cache entry that does not need mips.
  PaintImage image = CreatePaintImageInternal(gfx::Size(3072, 4096));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  // Cancel the task without ever using it.
  TestTileTaskRunner::CancelTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache->UnrefImage(draw_image);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());

  // Do an at-raster decode of the above image that *does* require mips.
  DrawImage draw_image_mips(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(0.6f, 0.6f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image_mips));
  cache->DrawWithImageFinished(draw_image_mips, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, OutOfRasterDecodeTask) {
  auto cache = CreateCache();

  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 1));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable);
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache->GetOutOfRasterDecodeTaskForImageAndRef(draw_image);
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(cache->IsInInUseCacheForTesting(draw_image));

  // Run the decode task.
  TestTileTaskRunner::ProcessTask(result.task.get());

  // The image should remain in the cache till we unref it.
  EXPECT_TRUE(cache->IsInInUseCacheForTesting(draw_image));
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, ZeroCacheNormalWorkingSet) {
  SetCachedTexturesLimit(0);
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Add an image to the cache-> Due to normal working set, this should produce
  // a task and a ref.
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the task.
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Request the same image - it should be cached.
  ImageDecodeCache::TaskResult second_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_FALSE(second_result.task);

  // Unref both images.
  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);

  // Ensure the unref is processed:
  cache->ReduceCacheUsage();

  // Get the image again. As it was fully unreffed, it is no longer in the
  // working set and will be evicted due to 0 cache size.
  ImageDecodeCache::TaskResult third_result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_EQ(third_result.task->dependencies().size(), 1u);
  EXPECT_TRUE(third_result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, SmallCacheNormalWorkingSet) {
  // Cache will fit one image.
  SetCachedTexturesLimit(1);
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  PaintImage image2 = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image2(
      image2, SkIRect::MakeWH(image2.width(), image2.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Add an image to the cache and un-ref it.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image);
  }

  // Request the same image - it should be cached.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);
    cache->UnrefImage(draw_image);
  }

  // Add a new image to the cache It should push out the old one.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image2, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image2);
  }

  // Request the second image - it should be cached.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image2, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);
    cache->UnrefImage(draw_image2);
  }

  // Request the first image - it should have been evicted and return a new
  // task.
  {
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image);
  }
}

TEST_P(GpuImageDecodeCacheTest, ClearCache) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  for (int i = 0; i < 10; ++i) {
    PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache->UnrefImage(draw_image);
  }

  // We should now have images in our cache.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 10u);

  // Tell our cache to clear resources.
  cache->ClearCache();

  // We should now have nothing in our cache.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, ClearCacheInUse) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create an image but keep it reffed so it can't be immediately freed.
  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // We should now have data image in our cache.
  EXPECT_GT(cache->GetWorkingSetBytesForTesting(), 0u);
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 1u);

  // Tell our cache to clear resources.
  cache->ClearCache();
  // We should still have data, as we can't clear the in-use entry.
  EXPECT_GT(cache->GetWorkingSetBytesForTesting(), 0u);
  // But the num (persistent) entries should be 0, as the entry is orphaned.
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 0u);

  // Unref the image, it should immidiately delete, leaving our cache empty.
  cache->UnrefImage(draw_image);
  EXPECT_EQ(cache->GetWorkingSetBytesForTesting(), 0u);
  EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageDifferentColorSpace) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  gfx::ColorSpace color_space_a = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace color_space_b = gfx::ColorSpace::CreateXYZD50();

  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_a);
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_b);
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_a);
  ImageDecodeCache::TaskResult third_result = cache->GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == first_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache->UnrefImage(first_draw_image);
  cache->UnrefImage(second_draw_image);
  cache->UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForLargeImageNonSRGBColorSpace) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateXYZD50();

  // Create an image that's too large to cache.
  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, color_space);
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, CacheDecodesExpectedFrames) {
  auto cache = CreateCache();

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5)),
  };
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::Make(10, 10, color_type_, kPremul_SkAlphaType), frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());

  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       1u, DefaultColorSpace());
  auto decoded_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();
  cache->DrawWithImageFinished(draw_image, decoded_image);

  // Scaled.
  DrawImage scaled_draw_image(draw_image, 0.5f, 2u,
                              draw_image.target_color_space());
  decoded_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(scaled_draw_image));
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(2u), 1u);
  generator->reset_frames_decoded();
  cache->DrawWithImageFinished(scaled_draw_image, decoded_image);

  // Subset.
  DrawImage subset_draw_image(
      image, SkIRect::MakeWH(5, 5), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable), 3u,
      DefaultColorSpace());
  decoded_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(subset_draw_image));
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(3u), 1u);
  generator->reset_frames_decoded();
  cache->DrawWithImageFinished(subset_draw_image, decoded_image);
}

TEST_P(GpuImageDecodeCacheTest, OrphanedDataCancelledWhileReplaced) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create a downscaled image.
  PaintImage first_image = CreatePaintImageInternal(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache->GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  // The cache should have exactly one image.
  EXPECT_EQ(1u, cache->GetNumCacheEntriesForTesting());

  // Create a larger version of |first_image|, this should immediately free
  // the memory used by |first_image| for the smaller scale.
  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  // The cache should have two images.
  EXPECT_EQ(1u, cache->GetNumCacheEntriesForTesting());

  // Cancel and unref the first image, it was orphaned, so it should be
  // immediately deleted.
  TestTileTaskRunner::CancelTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(first_result.task.get());
  TestTileTaskRunner::CompleteTask(first_result.task.get());
  cache->UnrefImage(first_draw_image);

  // The cache should have exactly one image.
  EXPECT_EQ(1u, cache->GetNumCacheEntriesForTesting());

  // Unref the second image. It is persistent, and should remain in the cache.
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());
  cache->UnrefImage(second_draw_image);

  // The cache should have exactly one image.
  EXPECT_EQ(1u, cache->GetNumCacheEntriesForTesting());
  EXPECT_EQ(0u, cache->GetInUseCacheEntriesForTesting());
}

TEST_P(GpuImageDecodeCacheTest, AlreadyBudgetedImagesAreNotAtRaster) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Allow a single 10x10 image and lock it.
  cache->SetWorkingSetLimitsForTesting(
      SkColorTypeBytesPerPixel(GetParam().first) * 10 * 10 * 10 /* max_bytes */,
      1 /* max_items */);
  PaintImage image = CreatePaintImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  // Try locking the same image again, its already budgeted so it shouldn't be
  // at-raster.
  result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, ImageBudgetingByCount) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Allow a single image by count. Use a high byte limit as we want to test the
  // count restriction.
  cache->SetWorkingSetLimitsForTesting(96 * 1024 * 1024 /* max_bytes */,
                                       1 /* max_items */);
  PaintImage image = CreatePaintImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // The image counts against our budget.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());

  // Try another image, it shouldn't be budgeted and should be at-raster.
  DrawImage second_draw_image(
      CreatePaintImageInternal(gfx::Size(100, 100)),
      SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  // Should be at raster.
  ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);
  // Image retrieved from at-raster decode should not be budgeted.
  DecodedDrawImage second_decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(second_draw_image));
  EXPECT_TRUE(second_decoded_draw_image.image());
  EXPECT_FALSE(second_decoded_draw_image.is_budgeted());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->DrawWithImageFinished(second_draw_image, second_decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, ImageBudgetingBySize) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Allow a single 10x10 image by size. Don't restrict the items limit as we
  // want to test the size limit.
  cache->SetWorkingSetLimitsForTesting(
      SkColorTypeBytesPerPixel(GetParam().first) * 10 * 10 * 10 /* max_bytes */,
      256 /* max_items */);
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // The image counts against our budget.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());

  // Try another image, it shouldn't be budgeted and should be at-raster.
  DrawImage second_draw_image(
      CreateDiscardablePaintImage(gfx::Size(100, 100)),
      SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  // Should be at raster.
  ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);
  // Image retrieved from at-raster decode should not be budgeted.
  DecodedDrawImage second_decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(second_draw_image));
  EXPECT_TRUE(second_decoded_draw_image.image());
  EXPECT_FALSE(second_decoded_draw_image.is_budgeted());

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->DrawWithImageFinished(second_draw_image, second_decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest,
       ColorConversionDuringDecodeForLargeImageNonSRGBColorSpace) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateXYZD50();

  // Create an image that's too large to upload.
  PaintImage image = CreatePaintImageInternal(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, color_space);
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      cache->GetDecodedImageForDraw(draw_image);

  sk_sp<SkColorSpace> target_color_space = cache->SupportsColorSpaceConversion()
                                               ? color_space.ToSkColorSpace()
                                               : nullptr;

  if (use_transfer_cache_) {
    // If using the transfer cache, the color conversion should be applied
    // there as well, even if it is a software image.
    sk_sp<SkImage> service_image = GetLastTransferredImage();
    ASSERT_TRUE(image);
    EXPECT_FALSE(service_image->isTextureBacked());
    EXPECT_EQ(image.width(), service_image->width());
    EXPECT_EQ(image.height(), service_image->height());

    // Color space should be logically equal to the original color space.
    EXPECT_TRUE(SkColorSpace::Equals(service_image->colorSpace(),
                                     target_color_space.get()));
  } else {
    sk_sp<SkImage> decoded_image =
        cache->GetSWImageDecodeForTesting(draw_image);
    // Ensure that the "uploaded" image we get back is the same as the decoded
    // image we've cached.
    EXPECT_TRUE(decoded_image == decoded_draw_image.image());
    // Ensure that the SW decoded image had colorspace conversion applied.
    EXPECT_TRUE(decoded_image->colorSpace() == target_color_space.get());
  }

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest,
       ColorConversionDuringUploadForSmallImageNonSRGBColorSpace) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateDisplayP3D65();

  PaintImage image = CreatePaintImageInternal(gfx::Size(11, 12));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, color_space);
  ImageDecodeCache::TaskResult result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      cache->GetDecodedImageForDraw(draw_image);

  sk_sp<SkColorSpace> target_color_space = cache->SupportsColorSpaceConversion()
                                               ? color_space.ToSkColorSpace()
                                               : nullptr;

  if (use_transfer_cache_) {
    // If using the transfer cache, the color conversion should be applied
    // there during upload.
    sk_sp<SkImage> service_image = GetLastTransferredImage();
    ASSERT_TRUE(image);
    EXPECT_TRUE(service_image->isTextureBacked());
    EXPECT_EQ(image.width(), service_image->width());
    EXPECT_EQ(image.height(), service_image->height());

    // Color space should be logically equal to the original color space.
    EXPECT_TRUE(SkColorSpace::Equals(service_image->colorSpace(),
                                     target_color_space.get()));
  } else {
    // Ensure that the HW uploaded image had color space conversion applied.
    EXPECT_TRUE(decoded_draw_image.image()->colorSpace() ==
                target_color_space.get());
  }

  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NonLazyImageUploadNoScale) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateBitmapImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  // For non-lazy images used at the original scale, no cpu component should be
  // cached
  EXPECT_FALSE(cache->GetSWImageDecodeForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, NonLazyImageUploadTaskHasNoDeps) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateBitmapImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  auto result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.task->dependencies().empty());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NonLazyImageUploadTaskCancelled) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateBitmapImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  auto result =
      cache->GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.task->dependencies().empty());
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache->UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NonLazyImageLargeImageColorConverted) {
  auto cache = CreateCache();
  const bool should_cache_sw_image =
      cache->SupportsColorSpaceConversion() && !use_transfer_cache_;

  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateBitmapImageInternal(gfx::Size(10, 24000));
  DrawImage draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, gfx::ColorSpace::CreateDisplayP3D65());
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  // For non-lazy images color converted during scaling, cpu component should be
  // cached.
  auto sw_image = cache->GetSWImageDecodeForTesting(draw_image);
  ASSERT_EQ(!!sw_image, should_cache_sw_image);
  if (should_cache_sw_image) {
    EXPECT_TRUE(SkColorSpace::Equals(
        sw_image->colorSpace(),
        gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace().get()));
  }
}

TEST_P(GpuImageDecodeCacheTest, NonLazyImageUploadDownscaled) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateBitmapImageInternal(gfx::Size(10, 10));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  DecodedDrawImage decoded_draw_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.is_budgeted());
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
  // For non-lazy images which are downscaled, the scaled image should be
  // cached.
  auto sw_image = cache->GetSWImageDecodeForTesting(draw_image);
  EXPECT_TRUE(sw_image);
  EXPECT_EQ(sw_image->width(), 5);
  EXPECT_EQ(sw_image->height(), 5);
}

TEST_P(GpuImageDecodeCacheTest, KeepOnlyLast2ContentIds) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  const PaintImage::Id paint_image_id = PaintImage::GetNextId();
  std::vector<DrawImage> draw_images;
  std::vector<DecodedDrawImage> decoded_draw_images;

  for (int i = 0; i < 10; ++i) {
    PaintImage image = CreatePaintImageInternal(
        gfx::Size(10, 10), SkColorSpace::MakeSRGB(), paint_image_id);
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), quality,
        CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));

    draw_images.push_back(draw_image);
    decoded_draw_images.push_back(decoded_draw_image);

    if (i == 0)
      continue;

    // We should only have the last 2 entries in the persistent cache, even
    // though everything is in the in use cache.
    EXPECT_EQ(cache->GetNumCacheEntriesForTesting(), 2u);
    EXPECT_EQ(cache->GetInUseCacheEntriesForTesting(), i + 1u);
    EXPECT_TRUE(cache->IsInPersistentCacheForTesting(draw_images[i]));
    EXPECT_TRUE(cache->IsInPersistentCacheForTesting(draw_images[i - 1]));
  }

  for (int i = 0; i < 10; ++i) {
    cache->DrawWithImageFinished(draw_images[i], decoded_draw_images[i]);
  }

  // We have a single tracked entry, that gets cleared once we purge the cache.
  EXPECT_EQ(cache->paint_image_entries_count_for_testing(), 1u);
  cache->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(cache->paint_image_entries_count_for_testing(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, DecodeToScale) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kMedium_SkFilterQuality;

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultColorSpace().ToSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  DrawImage draw_image1(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image1 =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image1));
  ASSERT_TRUE(decoded_image1.image());
  EXPECT_EQ(decoded_image1.image()->width(), 50);
  EXPECT_EQ(decoded_image1.image()->height(), 50);

  // We should have requested a scaled decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 50);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 50);
  cache->DrawWithImageFinished(draw_image1, decoded_image1);
}

TEST_P(GpuImageDecodeCacheTest, DecodeToScaleNoneQuality) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  SkFilterQuality quality = kNone_SkFilterQuality;

  viz::ContextProvider::ScopedContextLock context_lock(context_provider());
  SkISize full_size = SkISize::Make(100, 100);
  std::vector<SkISize> supported_sizes = {SkISize::Make(25, 25),
                                          SkISize::Make(50, 50)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height(),
                                     DefaultColorSpace().ToSkColorSpace()),
          frames, true, supported_sizes);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5, 0.5), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_image =
      EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
  ASSERT_TRUE(decoded_image.image());
  EXPECT_EQ(decoded_image.image()->width(), 50);
  EXPECT_EQ(decoded_image.image()->height(), 50);

  // We should have requested the original decode from the generator.
  ASSERT_EQ(generator->decode_infos().size(), 1u);
  EXPECT_EQ(generator->decode_infos().at(0).width(), 100);
  EXPECT_EQ(generator->decode_infos().at(0).height(), 100);
  cache->DrawWithImageFinished(draw_image, decoded_image);
}

TEST_P(GpuImageDecodeCacheTest, BasicMips) {
  auto decode_and_check_mips = [this](SkFilterQuality filter_quality,
                                      SkSize scale, gfx::ColorSpace color_space,
                                      bool should_have_mips) {
    auto cache = CreateCache();
    bool is_decomposable = true;

    PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));
    DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                         filter_quality, CreateMatrix(scale, is_decomposable),
                         PaintImage::kDefaultFrameIndex, color_space);
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());

    // Must hold context lock before calling GetDecodedImageForDraw /
    // DrawWithImageFinished.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
    EXPECT_TRUE(decoded_draw_image.image());
    EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());

    sk_sp<SkImage> image_with_mips =
        decoded_draw_image.image()->makeTextureImage(
            context_provider()->GrContext(), nullptr, GrMipMapped::kYes);
    EXPECT_EQ(should_have_mips, image_with_mips == decoded_draw_image.image());

    cache->DrawWithImageFinished(draw_image, decoded_draw_image);
    cache->UnrefImage(draw_image);
  };

  // No scale == no mips.
  decode_and_check_mips(kMedium_SkFilterQuality, SkSize::Make(1.0f, 1.0f),
                        DefaultColorSpace(), false);
  // Full mip level scale == no mips
  decode_and_check_mips(kMedium_SkFilterQuality, SkSize::Make(0.5f, 0.5f),
                        DefaultColorSpace(), false);
  // Low filter quality == no mips
  decode_and_check_mips(kLow_SkFilterQuality, SkSize::Make(0.6f, 0.6f),
                        DefaultColorSpace(), false);
  // None filter quality == no mips
  decode_and_check_mips(kNone_SkFilterQuality, SkSize::Make(0.6f, 0.6f),
                        DefaultColorSpace(), false);
  // Medium filter quality == mips
  decode_and_check_mips(kMedium_SkFilterQuality, SkSize::Make(0.6f, 0.6f),
                        DefaultColorSpace(), true);
  // High filter quality == mips
  decode_and_check_mips(kHigh_SkFilterQuality, SkSize::Make(0.6f, 0.6f),
                        DefaultColorSpace(), true);
  // Color conversion preserves mips
  decode_and_check_mips(kMedium_SkFilterQuality, SkSize::Make(0.6f, 0.6f),
                        gfx::ColorSpace::CreateXYZD50(), true);
}

TEST_P(GpuImageDecodeCacheTest, MipsAddedSubsequentDraw) {
  auto cache = CreateCache();
  bool is_decomposable = true;
  auto filter_quality = kMedium_SkFilterQuality;

  PaintImage image = CreatePaintImageInternal(gfx::Size(100, 100));

  // Create an image with no scaling. It will not have mips.
  {
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), filter_quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());

    // Must hold context lock before calling GetDecodedImageForDraw /
    // DrawWithImageFinished.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
    EXPECT_TRUE(decoded_draw_image.image());
    EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());

    // No mips should be generated
    sk_sp<SkImage> image_with_mips =
        decoded_draw_image.image()->makeTextureImage(
            context_provider()->GrContext(), nullptr, GrMipMapped::kYes);
    EXPECT_NE(image_with_mips, decoded_draw_image.image());

    cache->DrawWithImageFinished(draw_image, decoded_draw_image);
    cache->UnrefImage(draw_image);
  }

  // Call ReduceCacheUsage to clean up.
  cache->ReduceCacheUsage();

  // Request the same image again, but this time with a scale. We should get
  // no new task (re-uses the existing image), but mips should have been
  // added.
  {
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), filter_quality,
        CreateMatrix(SkSize::Make(0.6f, 0.6f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);

    // Must hold context lock before calling GetDecodedImageForDraw /
    // DrawWithImageFinished.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));

    EXPECT_TRUE(decoded_draw_image.image());
    EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());

    // Mips should be generated
    sk_sp<SkImage> image_with_mips =
        decoded_draw_image.image()->makeTextureImage(
            context_provider()->GrContext(), nullptr, GrMipMapped::kYes);
    EXPECT_EQ(image_with_mips, decoded_draw_image.image());
    cache->DrawWithImageFinished(draw_image, decoded_draw_image);
    cache->UnrefImage(draw_image);
  }
}

TEST_P(GpuImageDecodeCacheTest, MipsAddedWhileOriginalInUse) {
#if defined(OS_WIN)
  // TODO(ericrk): Mips are temporarily disabled to investigate a memory
  // regression on Windows. https://crbug.com/867468
  return;
#endif  // defined(OS_WIN)

  auto cache = CreateCache();
  bool is_decomposable = true;
  auto filter_quality = kMedium_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));

  struct Decode {
    DrawImage image;
    DecodedDrawImage decoded_image;
  };
  std::vector<Decode> images_to_unlock;

  // Create an image with no scaling. It will not have mips.
  {
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), filter_quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());

    // Must hold context lock before calling GetDecodedImageForDraw /
    // DrawWithImageFinished.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));
    EXPECT_TRUE(decoded_draw_image.image());
    EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());

    // No mips should be generated
    sk_sp<SkImage> image_with_mips =
        decoded_draw_image.image()->makeTextureImage(
            context_provider()->GrContext(), nullptr, GrMipMapped::kYes);
    EXPECT_NE(image_with_mips, decoded_draw_image.image());

    images_to_unlock.push_back({draw_image, decoded_draw_image});
  }

  // Second decode with mips.
  {
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), filter_quality,
        CreateMatrix(SkSize::Make(0.6f, 0.6f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache->GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);

    // Must hold context lock before calling GetDecodedImageForDraw /
    // DrawWithImageFinished.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    DecodedDrawImage decoded_draw_image =
        EnsureImageBacked(cache->GetDecodedImageForDraw(draw_image));

    EXPECT_TRUE(decoded_draw_image.image());
    EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());

    // Mips should be generated
    sk_sp<SkImage> image_with_mips =
        decoded_draw_image.image()->makeTextureImage(
            context_provider()->GrContext(), nullptr, GrMipMapped::kYes);
    EXPECT_EQ(image_with_mips, decoded_draw_image.image());

    images_to_unlock.push_back({draw_image, decoded_draw_image});
  }

  // Reduce cache usage to make sure anything marked for deletion is actually
  // deleted.
  cache->ReduceCacheUsage();

  {
    // All images which are currently ref-ed must have locked textures.
    viz::ContextProvider::ScopedContextLock context_lock(context_provider());
    for (const auto& decode : images_to_unlock) {
      if (!use_transfer_cache_) {
        discardable_manager_.ExpectLocked(GpuImageDecodeCache::GlIdFromSkImage(
            decode.decoded_image.image().get()));
      }
      cache->DrawWithImageFinished(decode.image, decode.decoded_image);
      cache->UnrefImage(decode.image);
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    GpuImageDecodeCacheTests,
    GpuImageDecodeCacheTest,
    ::testing::Values(
        std::make_pair(kN32_SkColorType, false /* use_transfer_cache */),
        std::make_pair(kARGB_4444_SkColorType, false /* use_transfer_cache */),
        std::make_pair(kRGBA_F16_SkColorType, false /* use_transfer_cache */),
        std::make_pair(kN32_SkColorType, true /* use_transfer_cache */),
        std::make_pair(kARGB_4444_SkColorType, true /* use_transfer_cache */),
        std::make_pair(kRGBA_F16_SkColorType, true /* use_transfer_cache */)));

}  // namespace
}  // namespace cc

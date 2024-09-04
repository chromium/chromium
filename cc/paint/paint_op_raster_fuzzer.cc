// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <cstdint>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/service/service_font_manager.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    base::EnableTerminationOnOutOfMemory();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator);
  }

  ~Environment() { base::DiscardableMemoryAllocator::SetInstance(nullptr); }

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator;
};

class FontSupport : public gpu::ServiceFontManager::Client {
 public:
  FontSupport() = default;
  ~FontSupport() override = default;

  // gpu::ServiceFontManager::Client implementation.
  scoped_refptr<gpu::Buffer> GetShmBuffer(uint32_t shm_id) override {
    auto it = buffers_.find(shm_id);
    if (it != buffers_.end()) {
      return it->second;
    }
    return CreateBuffer(shm_id);
  }
  void ReportProgress() override {}

 private:
  scoped_refptr<gpu::Buffer> CreateBuffer(uint32_t shm_id) {
    static const size_t kBufferSize = 2048u;
    base::UnsafeSharedMemoryRegion shared_memory =
        base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    base::WritableSharedMemoryMapping mapping = shared_memory.Map();
    auto buffer = gpu::MakeBufferFromSharedMemory(std::move(shared_memory),
                                                  std::move(mapping));
    buffers_[shm_id] = buffer;
    return buffer;
  }

  base::flat_map<uint32_t, scoped_refptr<gpu::Buffer>> buffers_;
};

void Raster(scoped_refptr<viz::TestContextProvider> context_provider,
            SkStrikeClient* strike_client,
            cc::ServicePaintCache* paint_cache,
            const uint8_t* data,
            size_t size) {
  const size_t kRasterDimension = 32;

  SkImageInfo image_info = SkImageInfo::MakeN32(
      kRasterDimension, kRasterDimension, kOpaque_SkAlphaType);
  context_provider->BindToCurrentSequence();
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      context_provider->GrContext(), skgpu::Budgeted::kYes, image_info);
  SkCanvas* canvas = surface->getCanvas();

  cc::PlaybackParams params(nullptr, canvas->getLocalToDevice());
  cc::TransferCacheTestHelper transfer_cache_helper;
  std::vector<uint8_t> scratch_buffer;
  cc::PaintOp::DeserializeOptions deserialize_options{
      .transfer_cache = &transfer_cache_helper,
      .paint_cache = paint_cache,
      .strike_client = strike_client,
      .scratch_buffer = scratch_buffer,
      .is_privileged = true};

  // Need kHeaderBytes bytes to be able to read the header.
  while (size >= cc::PaintOpWriter::kHeaderBytes) {
    std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
        static_cast<char*>(base::AlignedAlloc(
            sizeof(cc::LargestPaintOp), cc::PaintOpBuffer::kPaintOpAlign)));
    size_t bytes_read = 0;
    cc::PaintOp* deserialized_op = cc::PaintOp::Deserialize(
        data, size, deserialized.get(), sizeof(cc::LargestPaintOp), &bytes_read,
        deserialize_options);

    if (!deserialized_op) {
      break;
    }

    deserialized_op->Raster(canvas, params);

    deserialized_op->DestroyThis();

    size -= bytes_read;
    data += bytes_read;
  }
}

// Deserialize an arbitrary number of cc::PaintOps and raster them
// using gpu raster into an SkCanvas.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size <= sizeof(size_t)) {
    return 0;
  }

  [[maybe_unused]] static Environment* env = new Environment();
  base::CommandLine::Init(0, nullptr);

  // Partition the data to use some bytes for populating the font cache.
  uint32_t bytes_for_fonts = data[0];
  if (bytes_for_fonts > size) {
    bytes_for_fonts = size / 2;
  }
  const uint8_t* raster_data = base::bits::AlignDown(
      data + bytes_for_fonts, cc::PaintOpWriter::kMaxAlignment);
  if (raster_data < data) {
    return 0;
  }
  bytes_for_fonts = raster_data - data;
  size_t raster_size = size - bytes_for_fonts;

  FontSupport font_support;
  scoped_refptr<gpu::ServiceFontManager> font_manager(
      new gpu::ServiceFontManager(&font_support,
                                  false /* disable_oopr_debug_crash_dump */));
  cc::ServicePaintCache paint_cache;
  std::vector<SkDiscardableHandleId> locked_handles;
  if (bytes_for_fonts > 0u) {
    font_manager->Deserialize(data, bytes_for_fonts, &locked_handles);
  }

  auto context_provider_no_support = viz::TestContextProvider::Create();
  context_provider_no_support->BindToCurrentSequence();
  CHECK(!context_provider_no_support->GrContext()->supportsDistanceFieldText());
  Raster(context_provider_no_support, font_manager->strike_client(),
         &paint_cache, raster_data, raster_size);

  auto context_provider_with_support = viz::TestContextProvider::Create(
      std::string("GL_OES_standard_derivatives"));
  context_provider_with_support->BindToCurrentSequence();
  CHECK(
      context_provider_with_support->GrContext()->supportsDistanceFieldText());
  Raster(context_provider_with_support, font_manager->strike_client(),
         &paint_cache, raster_data, raster_size);

  font_manager->Unlock(locked_handles);
  font_manager->Destroy();
  return 0;
}

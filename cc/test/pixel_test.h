// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_H_
#define CC_TEST_PIXEL_TEST_H_

#include "base/files/file_util.h"
#include "base/single_thread_task_runner.h"
#include "cc/test/pixel_comparator.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"

namespace base {
class Thread;
}

namespace viz {
class CopyOutputResult;
class DirectRenderer;
class DisplayResourceProvider;
class GLRenderer;
class GpuServiceImpl;
class TestSharedBitmapManager;
}

namespace cc {
class FakeOutputSurfaceClient;
class OutputSurface;

class PixelTest : public testing::Test {
 protected:
  PixelTest();
  ~PixelTest() override;

  bool RunPixelTest(viz::RenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTest(viz::RenderPassList* pass_list,
                    std::vector<SkColor>* ref_pixels,
                    const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTarget(viz::RenderPassList* pass_list,
                                      viz::RenderPass* target,
                                      const base::FilePath& ref_file,
                                      const PixelComparator& comparator);

  bool RunPixelTestWithReadbackTargetAndArea(viz::RenderPassList* pass_list,
                                             viz::RenderPass* target,
                                             const base::FilePath& ref_file,
                                             const PixelComparator& comparator,
                                             const gfx::Rect* copy_rect);

  viz::ContextProvider* context_provider() const {
    return output_surface_->context_provider();
  }

  // Allocates a SharedMemory bitmap and registers it with the display
  // compositor's SharedBitmapManager.
  std::unique_ptr<base::SharedMemory> AllocateSharedBitmapMemory(
      const viz::SharedBitmapId& id,
      const gfx::Size& size);
  // Uses AllocateSharedBitmapMemory() then registers a ResourceId with the
  // |child_resource_provider_|, and copies the contents of |source| into the
  // software resource backing.
  viz::ResourceId AllocateAndFillSoftwareResource(const gfx::Size& size,
                                                  const SkBitmap& source);

  // For SkiaRendererDDL.
  std::unique_ptr<base::Thread> gpu_thread_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<viz::GpuServiceImpl> gpu_service_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor_;

  viz::RendererSettings renderer_settings_;
  gfx::Size device_viewport_size_;
  bool disable_picture_quad_image_filtering_;
  std::unique_ptr<FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<viz::OutputSurface> output_surface_;
  std::unique_ptr<viz::TestSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<viz::DisplayResourceProvider> resource_provider_;
  scoped_refptr<viz::ContextProvider> child_context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<viz::DirectRenderer> renderer_;
  viz::SoftwareRenderer* software_renderer_ = nullptr;
  std::unique_ptr<SkBitmap> result_bitmap_;

  void SetUpGLWithoutRenderer(bool flipped_output_surface);
  void SetUpGLRenderer(bool flipped_output_surface);
  void SetUpSkiaRenderer();
  void SetUpSkiaRendererDDL();
  void SetUpSoftwareRenderer();

  void TearDown() override;

  void EnableExternalStencilTest();

 private:
  void ReadbackResult(base::Closure quit_run_loop,
                      std::unique_ptr<viz::CopyOutputResult> result);

  bool PixelsMatchReference(const base::FilePath& ref_file,
                            const PixelComparator& comparator);
  void SetUpGpuServiceOnGpuThread(base::WaitableEvent* event);
  void TearDownGpuServiceOnGpuThread(base::WaitableEvent* event);

  std::unique_ptr<gl::DisableNullDrawGLBindings> enable_pixel_output_;
};

template<typename RendererType>
class RendererPixelTest : public PixelTest {
 public:
  RendererType* renderer() {
    return static_cast<RendererType*>(renderer_.get());
  }

  bool use_gpu() { return !!child_context_provider_; }

 protected:
  void SetUp() override;
};

// Wrappers to differentiate renderers where the the output surface and viewport
// have an externally determined size and offset.
class GLRendererWithExpandedViewport : public viz::GLRenderer {
 public:
  GLRendererWithExpandedViewport(
      const viz::RendererSettings* settings,
      viz::OutputSurface* output_surface,
      viz::DisplayResourceProvider* resource_provider,
      scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
      : viz::GLRenderer(settings,
                        output_surface,
                        resource_provider,
                        std::move(current_task_runner)) {}
};

class SoftwareRendererWithExpandedViewport : public viz::SoftwareRenderer {
 public:
  SoftwareRendererWithExpandedViewport(
      const viz::RendererSettings* settings,
      viz::OutputSurface* output_surface,
      viz::DisplayResourceProvider* resource_provider)
      : SoftwareRenderer(settings, output_surface, resource_provider) {}
};

class GLRendererWithFlippedSurface : public viz::GLRenderer {
 public:
  GLRendererWithFlippedSurface(
      const viz::RendererSettings* settings,
      viz::OutputSurface* output_surface,
      viz::DisplayResourceProvider* resource_provider,
      scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
      : viz::GLRenderer(settings,
                        output_surface,
                        resource_provider,
                        std::move(current_task_runner)) {}
};

class SkiaRendererDDL : public viz::SkiaRenderer {
 public:
  SkiaRendererDDL(const viz::RendererSettings* settings,
                  viz::OutputSurface* output_surface,
                  viz::DisplayResourceProvider* resource_provider,
                  viz::SkiaOutputSurface* skia_output_surface)
      : viz::SkiaRenderer(settings,
                          output_surface,
                          resource_provider,
                          skia_output_surface,
                          viz::SkiaRenderer::DrawMode::DDL) {}
};

template <>
inline void RendererPixelTest<viz::GLRenderer>::SetUp() {
  SetUpGLRenderer(false);
}

template<>
inline void RendererPixelTest<GLRendererWithExpandedViewport>::SetUp() {
  SetUpGLRenderer(false);
}

template <>
inline void RendererPixelTest<GLRendererWithFlippedSurface>::SetUp() {
  SetUpGLRenderer(true);
}

template <>
inline void RendererPixelTest<viz::SoftwareRenderer>::SetUp() {
  SetUpSoftwareRenderer();
}

template<>
inline void RendererPixelTest<SoftwareRendererWithExpandedViewport>::SetUp() {
  SetUpSoftwareRenderer();
}

template <>
inline void RendererPixelTest<viz::SkiaRenderer>::SetUp() {
  SetUpSkiaRenderer();
}

template <>
inline void RendererPixelTest<SkiaRendererDDL>::SetUp() {
  SetUpSkiaRendererDDL();
}

typedef RendererPixelTest<viz::GLRenderer> GLRendererPixelTest;
typedef RendererPixelTest<viz::SoftwareRenderer> SoftwareRendererPixelTest;
typedef RendererPixelTest<viz::SkiaRenderer> SkiaRendererPixelTest;

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_

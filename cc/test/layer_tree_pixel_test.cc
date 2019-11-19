// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/switches.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/gl_in_process_context.h"

using gpu::gles2::GLES2Interface;

namespace cc {

LayerTreePixelTest::LayerTreePixelTest()
    : pixel_comparator_(new ExactPixelComparator(true)),
      pending_texture_mailbox_callbacks_(0) {}

LayerTreePixelTest::~LayerTreePixelTest() = default;

std::unique_ptr<TestLayerTreeFrameSink>
LayerTreePixelTest::CreateLayerTreeFrameSink(
    const viz::RendererSettings& renderer_settings,
    double refresh_rate,
    scoped_refptr<viz::ContextProvider>,
    scoped_refptr<viz::RasterContextProvider>) {
  scoped_refptr<viz::TestInProcessContextProvider> compositor_context_provider;
  scoped_refptr<viz::TestInProcessContextProvider> worker_context_provider;
  if (!use_software_renderer()) {
    compositor_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            /*enable_oop_rasterization=*/false, /*support_locking=*/false);
    // With vulkan, OOPR has to be enabled.
    worker_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            /*enable_oop_rasterization=*/use_vulkan(),
            /*support_locking=*/true);
    // Bind worker context to main thread like it is in production. This is
    // needed to fully initialize the context. Compositor context is bound to
    // the impl thread in LayerTreeFrameSink::BindToCurrentThread().
    gpu::ContextResult result = worker_context_provider->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  }
  static constexpr bool disable_display_vsync = false;
  bool synchronous_composite =
      !HasImplThread() &&
      !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
  viz::RendererSettings test_settings = renderer_settings;
  // Keep texture sizes exactly matching the bounds of the RenderPass to avoid
  // floating point badness in texcoords.
  test_settings.dont_round_texture_sizes_for_pixel_tests = true;
  auto delegating_output_surface = std::make_unique<TestLayerTreeFrameSink>(
      compositor_context_provider, worker_context_provider,
      gpu_memory_buffer_manager(), test_settings, ImplThreadTaskRunner(),
      synchronous_composite, disable_display_vsync, refresh_rate);
  delegating_output_surface->SetEnlargePassTextureAmount(
      enlarge_texture_amount_);
  return delegating_output_surface;
}

std::unique_ptr<viz::SkiaOutputSurface>
LayerTreePixelTest::CreateDisplaySkiaOutputSurfaceOnThread() {
  // Set up the SkiaOutputSurfaceImpl.
  auto output_surface = viz::SkiaOutputSurfaceImpl::Create(
      std::make_unique<viz::SkiaOutputSurfaceDependencyImpl>(
          viz::TestGpuServiceHolder::GetInstance()->gpu_service(),
          gpu::kNullSurfaceHandle),
      viz::RendererSettings());
  return output_surface;
}

std::unique_ptr<viz::OutputSurface>
LayerTreePixelTest::CreateDisplayOutputSurfaceOnThread(
    scoped_refptr<viz::ContextProvider> compositor_context_provider) {
  std::unique_ptr<PixelTestOutputSurface> display_output_surface;
  if (renderer_type_ == RENDERER_GL) {
    // Pixel tests use a separate context for the Display to more closely
    // mimic texture transport from the renderer process to the Display
    // compositor.
    auto display_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            /*enable_oop_rasterization=*/false, /*support_locking=*/false);
    gpu::ContextResult result = display_context_provider->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);

    bool flipped_output_surface = false;
    display_output_surface = std::make_unique<PixelTestOutputSurface>(
        std::move(display_context_provider), flipped_output_surface);
  } else {
    EXPECT_EQ(RENDERER_SOFTWARE, renderer_type_);
    display_output_surface = std::make_unique<PixelTestOutputSurface>(
        std::make_unique<viz::SoftwareOutputDevice>());
  }
  return std::move(display_output_surface);
}

std::unique_ptr<viz::CopyOutputRequest>
LayerTreePixelTest::CreateCopyOutputRequest() {
  return std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&LayerTreePixelTest::ReadbackResult,
                     base::Unretained(this)));
}

void LayerTreePixelTest::ReadbackResult(
    std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_BITMAP);
  result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
  EndTest();
}

void LayerTreePixelTest::BeginTest() {
  Layer* target =
      readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
  if (!layer_tree_host()->IsUsingLayerLists()) {
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  } else {
    layer_tree_host()->property_trees()->effect_tree.AddCopyRequest(
        target->effect_tree_index(), CreateCopyOutputRequest());
    layer_tree_host()
        ->property_trees()
        ->effect_tree.Node(target->effect_tree_index())
        ->has_copy_request = true;
  }
  PostSetNeedsCommitToMainThread();
}

void LayerTreePixelTest::AfterTest() {
  // Bitmap comparison.
  if (ref_file_.empty()) {
    EXPECT_TRUE(
        MatchesBitmap(*result_bitmap_, expected_bitmap_, *pixel_comparator_));
    return;
  }

  // File comparison.
  base::FilePath test_data_dir;
  EXPECT_TRUE(
      base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir));
  base::FilePath ref_file_path = test_data_dir.Append(ref_file_);

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    EXPECT_TRUE(WritePNGFile(*result_bitmap_, ref_file_path, true));
  EXPECT_TRUE(MatchesPNGFile(*result_bitmap_,
                             ref_file_path,
                             *pixel_comparator_));
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::CreateSolidColorLayer(
    const gfx::Rect& rect, SkColor color) {
  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetIsDrawable(true);
  layer->SetHitTestable(true);
  layer->SetBounds(rect.size());
  layer->SetPosition(gfx::PointF(rect.origin()));
  layer->SetOffsetToTransformParent(
      gfx::Vector2dF(rect.origin().x(), rect.origin().y()));
  layer->SetBackgroundColor(color);
  return layer;
}

void LayerTreePixelTest::EndTest() {
  // Drop textures on the main thread so that they can be cleaned up and
  // the pending callbacks will fire.
  for (size_t i = 0; i < texture_layers_.size(); ++i) {
    texture_layers_[i]->ClearTexture();
  }

  TryEndTest();
}

void LayerTreePixelTest::InitializeSettings(LayerTreeSettings* settings) {
  settings->gpu_rasterization_forced = use_vulkan();
}

void LayerTreePixelTest::TryEndTest() {
  if (!result_bitmap_)
    return;
  if (pending_texture_mailbox_callbacks_)
    return;
  LayerTreeTest::EndTest();
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::
    CreateSolidColorLayerWithBorder(
        const gfx::Rect& rect, SkColor color,
        int border_width, SkColor border_color) {
  std::vector<scoped_refptr<SolidColorLayer>> layers;
  CreateSolidColorLayerPlusBorders(rect, color, border_width, border_color,
                                   layers);
  layers[0]->AddChild(layers[1]);
  layers[0]->AddChild(layers[2]);
  layers[0]->AddChild(layers[3]);
  layers[0]->AddChild(layers[4]);
  return layers[0];
}

void LayerTreePixelTest::CreateSolidColorLayerPlusBorders(
    const gfx::Rect& rect,
    SkColor color,
    int border_width,
    SkColor border_color,
    std::vector<scoped_refptr<SolidColorLayer>>& layers) {
  scoped_refptr<SolidColorLayer> layer = CreateSolidColorLayer(rect, color);
  scoped_refptr<SolidColorLayer> border_top = CreateSolidColorLayer(
      gfx::Rect(0, 0, rect.width(), border_width), border_color);
  scoped_refptr<SolidColorLayer> border_left = CreateSolidColorLayer(
      gfx::Rect(0,
                border_width,
                border_width,
                rect.height() - border_width * 2),
      border_color);
  scoped_refptr<SolidColorLayer> border_right =
      CreateSolidColorLayer(gfx::Rect(rect.width() - border_width,
                                      border_width,
                                      border_width,
                                      rect.height() - border_width * 2),
                            border_color);
  scoped_refptr<SolidColorLayer> border_bottom = CreateSolidColorLayer(
      gfx::Rect(0, rect.height() - border_width, rect.width(), border_width),
      border_color);
  layers.push_back(layer);
  layers.push_back(border_top);
  layers.push_back(border_left);
  layers.push_back(border_right);
  layers.push_back(border_bottom);
}

void LayerTreePixelTest::RunPixelTest(RendererType renderer_type,
                                      scoped_refptr<Layer> content_root,
                                      base::FilePath file_name) {
  renderer_type_ = renderer_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTest(RendererType renderer_type,
                                      scoped_refptr<Layer> content_root,
                                      const SkBitmap& expected_bitmap) {
  renderer_type_ = renderer_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = base::FilePath();
  expected_bitmap_ = expected_bitmap;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTestWithLayerList(RendererType renderer_type,
                                                   base::FilePath file_name) {
  renderer_type_ = renderer_type;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunSingleThreadedPixelTest(
    RendererType renderer_type,
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  renderer_type_ = renderer_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::SINGLE_THREADED);
}

void LayerTreePixelTest::RunPixelTestWithReadbackTarget(
    RendererType renderer_type,
    scoped_refptr<Layer> content_root,
    Layer* target,
    base::FilePath file_name) {
  renderer_type_ = renderer_type;
  content_root_ = content_root;
  readback_target_ = target;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::SetupTree() {
  if (layer_tree_host()->IsUsingLayerLists()) {
    // In layer list mode, content_root_ is not used. The subclass should call
    // SetInitialRootBounds() if needed.
    LayerTreeTest::SetupTree();
    return;
  }

  SetInitialRootBounds(content_root_->bounds());
  LayerTreeTest::SetupTree();
  layer_tree_host()->root_layer()->AddChild(content_root_);
}

SkBitmap LayerTreePixelTest::CopyMailboxToBitmap(
    const gfx::Size& size,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    const gfx::ColorSpace& color_space) {
  SkBitmap bitmap;
  std::unique_ptr<gpu::GLInProcessContext> context =
      viz::CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();

  if (sync_token.HasData())
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  GLuint texture_id = gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);

  GLuint fbo = 0;
  gl->GenFramebuffers(1, &fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  EXPECT_EQ(static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE),
            gl->CheckFramebufferStatus(GL_FRAMEBUFFER));

  std::unique_ptr<uint8_t[]> pixels(new uint8_t[size.GetArea() * 4]);
  gl->ReadPixels(0,
                 0,
                 size.width(),
                 size.height(),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.get());

  gl->DeleteFramebuffers(1, &fbo);
  gl->DeleteTextures(1, &texture_id);

  EXPECT_TRUE(color_space.IsValid());
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size.width(), size.height(),
                                                color_space.ToSkColorSpace()));

  uint8_t* out_pixels = static_cast<uint8_t*>(bitmap.getPixels());

  size_t row_bytes = size.width() * 4;
  size_t total_bytes = size.height() * row_bytes;
  for (size_t dest_y = 0; dest_y < total_bytes; dest_y += row_bytes) {
    // Flip Y axis.
    size_t src_y = total_bytes - dest_y - row_bytes;
    // Swizzle OpenGL -> Skia byte order.
    for (size_t x = 0; x < row_bytes; x += 4) {
      out_pixels[dest_y + x + SK_R32_SHIFT/8] = pixels.get()[src_y + x + 0];
      out_pixels[dest_y + x + SK_G32_SHIFT/8] = pixels.get()[src_y + x + 1];
      out_pixels[dest_y + x + SK_B32_SHIFT/8] = pixels.get()[src_y + x + 2];
      out_pixels[dest_y + x + SK_A32_SHIFT/8] = pixels.get()[src_y + x + 3];
    }
  }

  return bitmap;
}

void LayerTreePixelTest::Finish() {
  std::unique_ptr<gpu::GLInProcessContext> context =
      viz::CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();
  gl->Finish();
}

}  // namespace cc

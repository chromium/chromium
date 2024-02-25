// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/test_switches.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/test/test_types.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "skia/buildflags.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

using gpu::gles2::GLES2Interface;

namespace cc {

namespace {

TestRasterType GetDefaultRasterType(viz::RendererType renderer_type) {
  switch (renderer_type) {
    case viz::RendererType::kSoftware:
      return TestRasterType::kBitmap;
    case viz::RendererType::kSkiaVk:
    case viz::RendererType::kSkiaGraphiteDawn:
    case viz::RendererType::kSkiaGraphiteMetal:
      return TestRasterType::kGpu;
    default:
      return TestRasterType::kOneCopy;
  }
}

}  // namespace

LayerTreePixelTest::LayerTreePixelTest(viz::RendererType renderer_type)
    : LayerTreeTest(renderer_type),
      raster_type_(GetDefaultRasterType(renderer_type)),
      pixel_comparator_(
          std::make_unique<AlphaDiscardingExactPixelComparator>()),
      pending_texture_mailbox_callbacks_(0) {
#if BUILDFLAG(SKIA_USE_DAWN)
  dawnProcSetProcs(&dawn::native::GetProcs());
#endif
}

LayerTreePixelTest::~LayerTreePixelTest() = default;

std::unique_ptr<TestLayerTreeFrameSink>
LayerTreePixelTest::CreateLayerTreeFrameSink(
    const viz::RendererSettings& renderer_settings,
    double refresh_rate,
    scoped_refptr<viz::RasterContextProvider>,
    scoped_refptr<viz::RasterContextProvider>) {
  scoped_refptr<viz::TestInProcessContextProvider> compositor_context_provider;
  scoped_refptr<viz::TestInProcessContextProvider> worker_context_provider;
  if (!use_software_renderer()) {
    compositor_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            viz::TestContextType::kSoftwareRaster, /*support_locking=*/false);

    viz::TestContextType worker_ri_type;
    switch (raster_type()) {
      case TestRasterType::kGpu:
        worker_ri_type = viz::TestContextType::kGpuRaster;
        break;
      case TestRasterType::kOneCopy:
        worker_ri_type = viz::TestContextType::kSoftwareRaster;
        break;
      case TestRasterType::kZeroCopy:
        worker_ri_type = viz::TestContextType::kSoftwareRaster;
        break;
      case TestRasterType::kBitmap:
        NOTREACHED();
    }
    worker_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            worker_ri_type, /*support_locking=*/true);
    // Bind worker context to main thread like it is in production. This is
    // needed to fully initialize the context. Compositor context is bound to
    // the impl thread in LayerTreeFrameSink::BindToCurrentSequence().
    gpu::ContextResult result =
        worker_context_provider->BindToCurrentSequence();
    {
      viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
          worker_context_provider.get());
      max_texture_size_ =
          worker_context_provider->ContextCapabilities().max_texture_size;
    }
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  } else {
    max_texture_size_ =
        layer_tree_host()->GetSettings().max_render_buffer_bounds_for_sw;
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
      gpu_memory_buffer_manager(), test_settings, &debug_settings_,
      task_runner_provider(), synchronous_composite, disable_display_vsync,
      refresh_rate);
  delegating_output_surface->SetEnlargePassTextureAmount(
      enlarge_texture_amount_);
  return delegating_output_surface;
}

void LayerTreePixelTest::DrawLayersOnThread(LayerTreeHostImpl* host_impl) {
  // Verify that we're using Gpu rasterization or not as requested.
  if (!use_software_renderer()) {
    viz::RasterContextProvider* worker_context_provider =
        host_impl->layer_tree_frame_sink()->worker_context_provider();
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        worker_context_provider);
    EXPECT_EQ(use_accelerated_raster(),
              worker_context_provider->ContextCapabilities().gpu_rasterization);
    EXPECT_EQ(raster_type() == TestRasterType::kGpu,
              worker_context_provider->ContextCapabilities().gpu_rasterization);
  } else {
    EXPECT_EQ(TestRasterType::kBitmap, raster_type());
  }
  LayerTreeTest::DrawLayersOnThread(host_impl);
}

void LayerTreePixelTest::InitializeSettings(LayerTreeSettings* settings) {
  LayerTreeTest::InitializeSettings(settings);
  settings->gpu_rasterization_disabled = !use_accelerated_raster();
  settings->use_zero_copy = raster_type() == TestRasterType::kZeroCopy;
}

std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
LayerTreePixelTest::CreateDisplayControllerOnThread() {
  auto skia_deps = std::make_unique<viz::SkiaOutputSurfaceDependencyImpl>(
      viz::TestGpuServiceHolder::GetInstance()->gpu_service(),
      gpu::kNullSurfaceHandle);
  return std::make_unique<viz::DisplayCompositorMemoryAndTaskController>(
      std::move(skia_deps));
}

std::unique_ptr<viz::SkiaOutputSurface>
LayerTreePixelTest::CreateSkiaOutputSurfaceOnThread(
    viz::DisplayCompositorMemoryAndTaskController* display_controller) {
  // Set up the SkiaOutputSurfaceImpl.
  auto output_surface = viz::SkiaOutputSurfaceImpl::Create(
      display_controller, viz::RendererSettings(), &debug_settings_);
  return output_surface;
}

std::unique_ptr<viz::OutputSurface>
LayerTreePixelTest::CreateSoftwareOutputSurfaceOnThread() {
  EXPECT_EQ(viz::RendererType::kSoftware, renderer_type_);
  return std::make_unique<PixelTestOutputSurface>(
      std::make_unique<viz::SoftwareOutputDevice>());
}

std::unique_ptr<viz::CopyOutputRequest>
LayerTreePixelTest::CreateCopyOutputRequest() {
  return std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputResult::Destination::kSystemMemory,
      base::BindOnce(&LayerTreePixelTest::ReadbackResult,
                     base::Unretained(this)));
}

void LayerTreePixelTest::ReadbackResult(
    std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  EXPECT_EQ(result->destination(),
            viz::CopyOutputResult::Destination::kSystemMemory);
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  result_bitmap_ =
      std::make_unique<SkBitmap>(scoped_bitmap.GetOutScopedBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
  EndTest();
}

void LayerTreePixelTest::BeginTest() {
  Layer* target = readback_target_ ? readback_target_.get()
                                   : layer_tree_host()->root_layer();
  if (!layer_tree_host()->IsUsingLayerLists()) {
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  } else {
    layer_tree_host()->property_trees()->effect_tree_mutable().AddCopyRequest(
        target->effect_tree_index(), CreateCopyOutputRequest());
    layer_tree_host()
        ->property_trees()
        ->effect_tree_mutable()
        .Node(target->effect_tree_index())
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
  if (cmd->HasSwitch(switches::kRebaselinePixelTests))
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
  layer->SetOffsetToTransformParent(gfx::Vector2dF(rect.OffsetFromOrigin()));
  // CreateSolidColorLayer is only being used in tests, so we can live with this
  // SkColor converted to SkColor4f.
  layer->SetBackgroundColor(SkColor4f::FromColor(color));
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

void LayerTreePixelTest::RunPixelTest(scoped_refptr<Layer> content_root,
                                      base::FilePath file_name) {
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTest(scoped_refptr<Layer> content_root,
                                      const SkBitmap& expected_bitmap) {
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = base::FilePath();
  expected_bitmap_ = expected_bitmap;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTestWithLayerList(base::FilePath file_name) {
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunSingleThreadedPixelTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::SINGLE_THREADED);
}

void LayerTreePixelTest::RunPixelTestWithReadbackTarget(
    scoped_refptr<Layer> content_root,
    Layer* target,
    base::FilePath file_name) {
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

}  // namespace cc

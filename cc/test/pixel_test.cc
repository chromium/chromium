// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

namespace cc {
namespace {
void DeleteSharedImage(scoped_refptr<gpu::ClientSharedImage> shared_image,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  shared_image->UpdateDestructionSyncToken(sync_token);
}
}  // namespace

PixelTest::PixelTest(GraphicsBackend backend)
    : device_viewport_size_(gfx::Size(200, 200)),
      output_surface_client_(std::make_unique<FakeOutputSurfaceClient>()),
      graphics_backend_(backend) {
  // Keep texture sizes exactly matching the bounds of the RenderPass to avoid
  // floating point badness in texcoords.
  renderer_settings_.dont_round_texture_sizes_for_pixel_tests = true;

  // Copy requests force full damage, but OutputSurface-based readback can test
  // incremental damage cases.
  renderer_settings_.partial_swap_enabled = true;

  // Check if the graphics backend needs to initialize Vulkan, Dawn.
  bool init_vulkan = false;
  bool init_dawn = false;
  if (backend == kSkiaVulkan) {
    scoped_feature_list_.InitAndEnableFeature(features::kVulkan);
    init_vulkan = true;
  } else if (backend == kSkiaGraphiteDawn) {
    scoped_feature_list_.InitAndEnableFeature(features::kSkiaGraphite);
    auto* command_line = base::CommandLine::ForCurrentProcess();
    bool use_gpu = command_line->HasSwitch(::switches::kUseGpuInTests);
    // Force the use of Graphite even if disallowed for other reasons e.g.
    // ANGLE Metal is not enabled on Mac. Use dawn-swiftshader backend if
    // kUseGpuInTests is not set.
    command_line->AppendSwitch(::switches::kEnableSkiaGraphite);
    command_line->AppendSwitchASCII(
        ::switches::kSkiaGraphiteBackend,
        use_gpu ? ::switches::kSkiaGraphiteBackendDawn
                : ::switches::kSkiaGraphiteBackendDawnSwiftshader);
    init_dawn = true;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    init_vulkan = true;
#endif
  } else if (backend == kSkiaGraphiteMetal) {
    scoped_feature_list_.InitAndEnableFeature(features::kSkiaGraphite);
    auto* command_line = base::CommandLine::ForCurrentProcess();
    // Force the use of Graphite even if disallowed for other reasons.
    command_line->AppendSwitch(::switches::kEnableSkiaGraphite);
    command_line->AppendSwitchASCII(::switches::kSkiaGraphiteBackend,
                                    ::switches::kSkiaGraphiteBackendMetal);
  } else {
    // Ensure that we don't accidentally have vulkan or graphite enabled.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kVulkan, features::kSkiaGraphite});
  }

  if (init_vulkan) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    bool use_gpu = command_line->HasSwitch(::switches::kUseGpuInTests);
    command_line->AppendSwitchASCII(
        ::switches::kUseVulkan,
        use_gpu ? ::switches::kVulkanImplementationNameNative
                : ::switches::kVulkanImplementationNameSwiftshader);
  }

  if (init_dawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    dawnProcSetProcs(&dawn::native::GetProcs());
#endif
  }
}

PixelTest::~PixelTest() = default;

void PixelTest::RenderReadbackTargetAndAreaToResultBitmap(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPass* target,
    const gfx::Rect* copy_rect) {
  base::RunLoop run_loop;

  const bool use_copy_request = target != pass_list->back().get();
  if (use_copy_request) {
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                           run_loop.QuitClosure()));
    if (copy_rect) {
      request->set_area(*copy_rect);
    }
    target->copy_requests.push_back(std::move(request));
  }

  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_,
                       display_color_spaces_,
                       std::move(surface_damage_rect_list_));

  if (use_copy_request) {
    // Call SwapBuffersSkipped(), so the renderer can have a chance to release
    // resources.
    renderer_->SwapBuffersSkipped();
  } else {
    renderer_->SwapBuffers(viz::DirectRenderer::SwapFrameData());
    output_surface_->ReadbackForTesting(
        base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                       run_loop.QuitClosure()));
  }

  // Wait for the readback to complete.
  run_loop.Run();
}

bool PixelTest::RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithCopyOutputRequest(pass_list, pass_list->back().get(),
                                           ref_file, comparator);
}

bool PixelTest::RunPixelTestWithCopyOutputRequest(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  return RunPixelTestWithCopyOutputRequestAndArea(pass_list, target, ref_file,
                                                  comparator, nullptr);
}

bool PixelTest::RunPixelTestWithCopyOutputRequestAndArea(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator,
    const gfx::Rect* copy_rect) {
  RenderReadbackTargetAndAreaToResultBitmap(pass_list, target, copy_rect);

  base::FilePath test_data_dir;
  if (!base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir)) {
    return false;
  }

  // If this is false, we didn't set up a readback on a render pass.
  if (!result_bitmap_) {
    return false;
  }

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kRebaselinePixelTests)) {
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);
  }

  return MatchesPNGFile(*result_bitmap_, test_data_dir.Append(ref_file),
                        comparator);
}

bool PixelTest::RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                             std::vector<SkColor>* ref_pixels,
                             const PixelComparator& comparator) {
  RenderReadbackTargetAndAreaToResultBitmap(pass_list, pass_list->back().get(),
                                            nullptr);

  // Need to wrap |ref_pixels| in a SkBitmap.
  DCHECK_EQ(ref_pixels->size(), static_cast<size_t>(result_bitmap_->width() *
                                                    result_bitmap_->height()));
  SkBitmap ref_pixels_bitmap;
  ref_pixels_bitmap.installPixels(
      SkImageInfo::MakeN32Premul(result_bitmap_->width(),
                                 result_bitmap_->height()),
      ref_pixels->data(), result_bitmap_->width() * sizeof(SkColor));
  bool result = comparator.Compare(*result_bitmap_, ref_pixels_bitmap);
  if (!result) {
    std::string res_bmp_data_url = GetPNGDataUrl(*result_bitmap_);
    std::string ref_bmp_data_url = GetPNGDataUrl(ref_pixels_bitmap);
    LOG(ERROR) << "Pixels do not match!";
    LOG(ERROR) << "Actual: " << res_bmp_data_url;
    LOG(ERROR) << "Expected: " << ref_bmp_data_url;
  }
  return result;
}

bool PixelTest::RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                             SkBitmap ref_bitmap,
                             const PixelComparator& comparator) {
  RenderReadbackTargetAndAreaToResultBitmap(pass_list, pass_list->back().get(),
                                            nullptr);

  bool result = comparator.Compare(*result_bitmap_, ref_bitmap);
  if (!result) {
    std::string res_bmp_data_url = GetPNGDataUrl(*result_bitmap_);
    std::string ref_bmp_data_url = GetPNGDataUrl(ref_bitmap);
    LOG(ERROR) << "Pixels do not match!";
    LOG(ERROR) << "Actual: " << res_bmp_data_url;
    LOG(ERROR) << "Expected: " << ref_bmp_data_url;
  }
  return result;
}

void PixelTest::ReadbackResult(base::OnceClosure quit_run_loop,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  EXPECT_EQ(result->destination(),
            viz::CopyOutputResult::Destination::kSystemMemory);
  auto scoped_sk_bitmap = result->ScopedAccessSkBitmap();
  result_bitmap_ =
      std::make_unique<SkBitmap>(scoped_sk_bitmap.GetOutScopedBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
  std::move(quit_run_loop).Run();
}

void PixelTest::AllocateSharedBitmapMemory(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& size,
    scoped_refptr<gpu::ClientSharedImage>& shared_image,
    base::WritableSharedMemoryMapping& mapping,
    gpu::SyncToken& sync_token) {
  DCHECK(context_provider);
  auto* shared_image_interface = context_provider->SharedImageInterface();
  DCHECK(shared_image_interface);
  auto shared_image_mapping = shared_image_interface->CreateSharedImage(
      {viz::SinglePlaneFormat::kBGRA_8888, size, gfx::ColorSpace(),
       gpu::SHARED_IMAGE_USAGE_CPU_WRITE, "PixelTestSharedBitmap"});

  shared_image = std::move(shared_image_mapping.shared_image);
  mapping = std::move(shared_image_mapping.mapping);
  sync_token = shared_image_interface->GenVerifiedSyncToken();
  CHECK(shared_image);
}

viz::ResourceId PixelTest::AllocateAndFillSoftwareResource(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& size,
    const SkBitmap& source) {
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  base::WritableSharedMemoryMapping mapping;
  gpu::SyncToken sync_token;
  AllocateSharedBitmapMemory(context_provider, size, shared_image, mapping,
                             sync_token);

  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  const size_t row_bytes = info.minRowBytes();
  base::span<uint8_t> mem(mapping);
  CHECK_GE(mem.size(), info.computeByteSize(row_bytes));
  source.readPixels(info, mem.data(), row_bytes, 0, 0);

  auto transferable_resource =
      viz::TransferableResource::MakeSoftwareSharedImage(
          shared_image, sync_token, size, viz::SinglePlaneFormat::kBGRA_8888,
          viz::TransferableResource::ResourceSource::kTileRasterTask);
  auto release_callback =
      base::BindOnce(&DeleteSharedImage, std::move(shared_image));

  return child_resource_provider_->ImportResource(
      std::move(transferable_resource), std::move(release_callback));
}

void PixelTest::SetUpSkiaRenderer(gfx::SurfaceOrigin output_surface_origin) {
  enable_pixel_output_ = std::make_unique<gl::DisableNullDrawGLBindings>();
  // Set up the GPU service.
  gpu_service_holder_ = viz::TestGpuServiceHolder::GetInstance();

  auto skia_deps = std::make_unique<viz::SkiaOutputSurfaceDependencyImpl>(
      gpu_service(), gpu::kNullSurfaceHandle);
  display_controller_ =
      std::make_unique<viz::DisplayCompositorMemoryAndTaskController>(
          std::move(skia_deps));
  output_surface_ = viz::SkiaOutputSurfaceImpl::Create(
      display_controller_.get(), renderer_settings_, &debug_settings_);
  output_surface_->BindToClient(output_surface_client_.get());
  static_cast<viz::SkiaOutputSurfaceImpl*>(output_surface_.get())
      ->SetCapabilitiesForTesting(output_surface_origin);
  auto resource_provider = std::make_unique<viz::DisplayResourceProviderSkia>();
  renderer_ = std::make_unique<viz::SkiaRenderer>(
      &renderer_settings_, &debug_settings_, output_surface_.get(),
      resource_provider.get(), nullptr,
      static_cast<viz::SkiaOutputSurface*>(output_surface_.get()));
  resource_provider_ = std::move(resource_provider);

  FinishSetup();
}

void PixelTest::TearDown() {
  // Tear down the client side context provider, etc.
  child_resource_provider_->ShutdownAndReleaseAllResources();
  child_resource_provider_.reset();
  child_context_provider_.reset();

  // Tear down the skia renderer.
  software_renderer_ = nullptr;
  renderer_.reset();
  resource_provider_.reset();
  output_surface_.reset();
}

void PixelTest::SetUpSoftwareRenderer() {
  // Set up the GPU service. It's only used for shared bitmaps but it's still
  // needed.
  gpu_service_holder_ = viz::TestGpuServiceHolder::GetInstance();

  output_surface_ = std::make_unique<PixelTestOutputSurface>(
      std::make_unique<viz::SoftwareOutputDevice>());
  output_surface_->BindToClient(output_surface_client_.get());

  auto* gpu_service = gpu_service_holder_->gpu_service();
  auto resource_provider =
      std::make_unique<viz::DisplayResourceProviderSoftware>(
          /*shared_bitmap_manager=*/nullptr,
          gpu_service->shared_image_manager(),
          gpu_service->sync_point_manager(), gpu_service->gpu_scheduler());

  auto renderer = std::make_unique<viz::SoftwareRenderer>(
      &renderer_settings_, &debug_settings_, output_surface_.get(),
      resource_provider.get(), nullptr);
  resource_provider_ = std::move(resource_provider);
  software_renderer_ = renderer.get();
  renderer_ = std::move(renderer);

  FinishSetup();
}

void PixelTest::FinishSetup() {
  CHECK(renderer_);
  renderer_->Initialize();
  renderer_->SetVisible(true);

  child_context_provider_ =
      base::MakeRefCounted<viz::TestInProcessContextProvider>(
          viz::TestContextType::kSoftwareRaster, /*support_locking=*/false);
  gpu::ContextResult result = child_context_provider_->BindToCurrentSequence();
  CHECK_EQ(result, gpu::ContextResult::kSuccess);
  child_resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
}

}  // namespace cc

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "skia/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

namespace cc {

PixelTest::PixelTest(GraphicsBackend backend)
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      output_surface_client_(std::make_unique<FakeOutputSurfaceClient>()) {
  // Keep texture sizes exactly matching the bounds of the RenderPass to avoid
  // floating point badness in texcoords.
  renderer_settings_.dont_round_texture_sizes_for_pixel_tests = true;

  // Check if the graphics backend needs to initialize Vulkan, Dawn.
  bool init_vulkan = false;
  bool init_dawn = false;
  if (backend == kSkiaVulkan) {
    scoped_feature_list_.InitAndEnableFeature(features::kVulkan);
    init_vulkan = true;
  } else if (backend == kSkiaGraphite) {
    scoped_feature_list_.InitAndEnableFeature(features::kSkiaGraphite);

    // Force the use of Graphite even if disallowed for other reasons e.g. ANGLE
    // Metal is not enabled on Mac.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(::switches::kSkiaGraphiteBackend);

    init_dawn = true;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    init_vulkan = true;
#endif
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

bool PixelTest::RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list, pass_list->back().get(),
                                        ref_file, comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTargetAndArea(
      pass_list, target, ref_file, comparator, nullptr);
}

bool PixelTest::RunPixelTestWithReadbackTargetAndArea(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator,
    const gfx::Rect* copy_rect) {
  base::RunLoop run_loop;

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  if (copy_rect)
    request->set_area(*copy_rect);
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_,
                       display_color_spaces_,
                       std::move(surface_damage_rect_list_));

  // Call SwapBuffersSkipped(), so the renderer can have a chance to release
  // resources.
  renderer_->SwapBuffersSkipped();

  // Wait for the readback to complete.
  run_loop.Run();

  return PixelsMatchReference(ref_file, comparator);
}

bool PixelTest::RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                             std::vector<SkColor>* ref_pixels,
                             const PixelComparator& comparator) {
  base::RunLoop run_loop;
  auto* target = pass_list->back().get();

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_,
                       display_color_spaces_,
                       std::move(surface_damage_rect_list_));

  // Call SwapBuffersSkipped(), so the renderer can have a chance to release
  // resources.
  renderer_->SwapBuffersSkipped();

  // Wait for the readback to complete.
  run_loop.Run();

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

bool PixelTest::PixelsMatchReference(const base::FilePath& ref_file,
                                     const PixelComparator& comparator) {
  base::FilePath test_data_dir;
  if (!base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir))
    return false;

  // If this is false, we didn't set up a readback on a render pass.
  if (!result_bitmap_)
    return false;

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kRebaselinePixelTests))
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(
      *result_bitmap_, test_data_dir.Append(ref_file), comparator);
}

base::WritableSharedMemoryMapping PixelTest::AllocateSharedBitmapMemory(
    const viz::SharedBitmapId& id,
    const gfx::Size& size) {
  base::MappedReadOnlyRegion shm = viz::bitmap_allocation::AllocateSharedBitmap(
      size, viz::SinglePlaneFormat::kRGBA_8888);
  this->shared_bitmap_manager_->ChildAllocatedSharedBitmap(shm.region.Map(),
                                                           id);
  return std::move(shm.mapping);
}

viz::ResourceId PixelTest::AllocateAndFillSoftwareResource(
    const gfx::Size& size,
    const SkBitmap& source) {
  viz::SharedBitmapId shared_bitmap_id = viz::SharedBitmap::GenerateId();
  base::WritableSharedMemoryMapping mapping =
      AllocateSharedBitmapMemory(shared_bitmap_id, size);

  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  source.readPixels(info, mapping.memory(), info.minRowBytes(), 0, 0);

  return child_resource_provider_->ImportResource(
      viz::TransferableResource::MakeSoftware(
          shared_bitmap_id, size, viz::SinglePlaneFormat::kRGBA_8888),
      base::DoNothing());
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
  renderer_->Initialize();
  renderer_->SetVisible(true);

  // Set up the client side context provider, etc
  child_context_provider_ =
      base::MakeRefCounted<viz::TestInProcessContextProvider>(
          viz::TestContextType::kGLES2WithRaster, /*support_locking=*/false);
  gpu::ContextResult result = child_context_provider_->BindToCurrentSequence();
  DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  child_resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
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
  output_surface_ = std::make_unique<PixelTestOutputSurface>(
      std::make_unique<viz::SoftwareOutputDevice>());
  output_surface_->BindToClient(output_surface_client_.get());
  shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
  auto resource_provider =
      std::make_unique<viz::DisplayResourceProviderSoftware>(
          shared_bitmap_manager_.get());
  child_resource_provider_ = std::make_unique<viz::ClientResourceProvider>();

  auto renderer = std::make_unique<viz::SoftwareRenderer>(
      &renderer_settings_, &debug_settings_, output_surface_.get(),
      resource_provider.get(), nullptr);
  resource_provider_ = std::move(resource_provider);
  software_renderer_ = renderer.get();
  renderer_ = std::move(renderer);
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

}  // namespace cc

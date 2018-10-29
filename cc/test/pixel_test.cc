// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/viz/privileged/interfaces/gl/gpu_host.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"

namespace cc {

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      output_surface_client_(std::make_unique<FakeOutputSurfaceClient>()) {
  // Keep texture sizes exactly matching the bounds of the RenderPass to avoid
  // floating point badness in texcoords.
  renderer_settings_.dont_round_texture_sizes_for_pixel_tests = true;
}

PixelTest::~PixelTest() = default;

bool PixelTest::RunPixelTest(viz::RenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list, pass_list->back().get(),
                                        ref_file, comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    viz::RenderPassList* pass_list,
    viz::RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTargetAndArea(
      pass_list, target, ref_file, comparator, nullptr);
}

bool PixelTest::RunPixelTestWithReadbackTargetAndArea(
    viz::RenderPassList* pass_list,
    viz::RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator,
    const gfx::Rect* copy_rect) {
  base::RunLoop run_loop;

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  if (copy_rect)
    request->set_area(*copy_rect);
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_);

  // Wait for the readback to complete.
  if (output_surface_->context_provider())
    output_surface_->context_provider()->ContextGL()->Finish();
  run_loop.Run();

  return PixelsMatchReference(ref_file, comparator);
}

bool PixelTest::RunPixelTest(viz::RenderPassList* pass_list,
                             std::vector<SkColor>* ref_pixels,
                             const PixelComparator& comparator) {
  base::RunLoop run_loop;
  viz::RenderPass* target = pass_list->back().get();

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_);

  // Wait for the readback to complete.
  if (output_surface_->context_provider())
    output_surface_->context_provider()->ContextGL()->Finish();
  run_loop.Run();

  // Need to wrap |ref_pixels| in a SkBitmap.
  DCHECK_EQ(ref_pixels->size(), static_cast<size_t>(result_bitmap_->width() *
                                                    result_bitmap_->height()));
  SkBitmap ref_pixels_bitmap;
  ref_pixels_bitmap.installPixels(
      SkImageInfo::MakeN32Premul(result_bitmap_->width(),
                                 result_bitmap_->height()),
      ref_pixels->data(), result_bitmap_->width() * sizeof(SkColor));
  return comparator.Compare(*result_bitmap_, ref_pixels_bitmap);
}

void PixelTest::ReadbackResult(base::Closure quit_run_loop,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_BITMAP);
  result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
  quit_run_loop.Run();
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
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(
      *result_bitmap_, test_data_dir.Append(ref_file), comparator);
}

std::unique_ptr<base::SharedMemory> PixelTest::AllocateSharedBitmapMemory(
    const viz::SharedBitmapId& id,
    const gfx::Size& size) {
  std::unique_ptr<base::SharedMemory> shm =
      viz::bitmap_allocation::AllocateMappedBitmap(size, viz::RGBA_8888);
  this->shared_bitmap_manager_->ChildAllocatedSharedBitmap(
      viz::bitmap_allocation::DuplicateAndCloseMappedBitmap(shm.get(), size,
                                                            viz::RGBA_8888),
      id);
  return shm;
}

viz::ResourceId PixelTest::AllocateAndFillSoftwareResource(
    const gfx::Size& size,
    const SkBitmap& source) {
  viz::SharedBitmapId shared_bitmap_id = viz::SharedBitmap::GenerateId();
  std::unique_ptr<base::SharedMemory> shm =
      AllocateSharedBitmapMemory(shared_bitmap_id, size);

  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  source.readPixels(info, shm->memory(), info.minRowBytes(), 0, 0);

  return child_resource_provider_->ImportResource(
      viz::TransferableResource::MakeSoftware(shared_bitmap_id, size,
                                              viz::RGBA_8888),
      viz::SingleReleaseCallback::Create(base::DoNothing()));
}

void PixelTest::SetUpGLWithoutRenderer(bool flipped_output_surface) {
  enable_pixel_output_ = std::make_unique<gl::DisableNullDrawGLBindings>();

  auto context_provider = base::MakeRefCounted<TestInProcessContextProvider>(
      /*enable_oop_rasterization=*/false, /*support_locking=*/false);
  gpu::ContextResult result = context_provider->BindToCurrentThread();
  DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  output_surface_ = std::make_unique<PixelTestOutputSurface>(
      std::move(context_provider), flipped_output_surface);
  output_surface_->BindToClient(output_surface_client_.get());

  shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
  resource_provider_ = std::make_unique<viz::DisplayResourceProvider>(
      viz::DisplayResourceProvider::kGpu, output_surface_->context_provider(),
      shared_bitmap_manager_.get());

  child_context_provider_ = base::MakeRefCounted<TestInProcessContextProvider>(
      /*enable_oop_rasterization=*/false, /*support_locking=*/false);
  result = child_context_provider_->BindToCurrentThread();
  DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  child_resource_provider_ =
      std::make_unique<viz::ClientResourceProvider>(true);
}

void PixelTest::SetUpGLRenderer(bool flipped_output_surface) {
  SetUpGLWithoutRenderer(flipped_output_surface);
  renderer_ = std::make_unique<viz::GLRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get(),
      base::ThreadTaskRunnerHandle::Get());
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

void PixelTest::SetUpSkiaRenderer() {
  SetUpGLWithoutRenderer(false);
  renderer_ = std::make_unique<viz::SkiaRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get(),
      nullptr /* skia_output_surface */, viz::SkiaRenderer::DrawMode::GL);
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

void PixelTest::SetUpGpuServiceOnGpuThread(base::WaitableEvent* event) {
  ASSERT_TRUE(gpu_thread_->task_runner()->BelongsToCurrentThread());
  gpu_service_ = std::make_unique<viz::GpuServiceImpl>(
      gpu::GPUInfo(), nullptr /* watchdog_thread */, io_thread_->task_runner(),
      gpu::GpuFeatureInfo(), gpu::GpuPreferences(), gpu::GPUInfo(),
      gpu::GpuFeatureInfo(), nullptr /* vulkan_implementation */,
      base::DoNothing() /* exit_callback */);

  // Uses a null gpu_host here, because we don't want to receive any message.
  std::unique_ptr<viz::mojom::GpuHost> gpu_host;
  viz::mojom::GpuHostPtr gpu_host_proxy;
  mojo::MakeStrongBinding(std::move(gpu_host),
                          mojo::MakeRequest(&gpu_host_proxy));
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      nullptr /* sync_point_manager */, nullptr /* shutdown_event */);
  task_executor_ = base::MakeRefCounted<gpu::GpuInProcessThreadService>(
      gpu_thread_->task_runner(), gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->share_group(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences());
  event->Signal();
}

void PixelTest::SetUpSkiaRendererDDL() {
  // Set up the GPU service.
  gpu_thread_ = std::make_unique<base::Thread>("GPUMainThread");
  ASSERT_TRUE(gpu_thread_->Start());
  io_thread_ = std::make_unique<base::Thread>("GPUIOThread");
  ASSERT_TRUE(io_thread_->Start());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PixelTest::SetUpGpuServiceOnGpuThread,
                                base::Unretained(this), &event));
  event.Wait();

  // Set up the skia renderer.
  output_surface_ = std::make_unique<viz::SkiaOutputSurfaceImpl>(
      gpu_service_.get(), gpu::kNullSurfaceHandle,
      nullptr /* synthetic_begin_frame_source */);
  output_surface_->BindToClient(output_surface_client_.get());
  resource_provider_ = std::make_unique<viz::DisplayResourceProvider>(
      viz::DisplayResourceProvider::kGpu,
      nullptr /* compositor_context_provider */,
      nullptr /* shared_bitmap_manager */);
  renderer_ = std::make_unique<viz::SkiaRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get(),
      static_cast<viz::SkiaOutputSurfaceImpl*>(output_surface_.get()),
      viz::SkiaRenderer::DrawMode::DDL);
  renderer_->Initialize();
  renderer_->SetVisible(true);

  // Set up the client side context provider, etc
  auto* gpu_channel_manager = gpu_service_->gpu_channel_manager();
  gpu_memory_buffer_manager_ =
      std::make_unique<viz::InProcessGpuMemoryBufferManager>(
          gpu_channel_manager);
  gpu::ImageFactory* image_factory = nullptr;
  if (gpu_channel_manager->gpu_memory_buffer_factory()) {
    image_factory =
        gpu_channel_manager->gpu_memory_buffer_factory()->AsImageFactory();
  }
  auto* gpu_channel_manager_delegate = gpu_channel_manager->delegate();
  child_context_provider_ =
      base::MakeRefCounted<viz::VizProcessContextProvider>(
          task_executor_, gpu::kNullSurfaceHandle,
          gpu_memory_buffer_manager_.get(), image_factory,
          gpu_channel_manager_delegate, gpu::SharedMemoryLimits(),
          false /* requires_alpha_channel */);
  child_context_provider_->BindToCurrentThread();
  child_resource_provider_ =
      std::make_unique<viz::ClientResourceProvider>(true);
}

void PixelTest::TearDownGpuServiceOnGpuThread(base::WaitableEvent* event) {
  task_executor_ = nullptr;
  gpu_service_ = nullptr;
  event->Signal();
}

void PixelTest::TearDown() {
  // Tear down the client side context provider, etc.
  child_resource_provider_->ShutdownAndReleaseAllResources();
  child_resource_provider_ = nullptr;
  child_context_provider_ = nullptr;
  gpu_memory_buffer_manager_ = nullptr;

  // Tear down the skia renderer.
  renderer_ = nullptr;
  resource_provider_ = nullptr;
  output_surface_ = nullptr;

  if (task_executor_) {
    // Tear down the GPU service.
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    gpu_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PixelTest::TearDownGpuServiceOnGpuThread,
                                  base::Unretained(this), &event));
    event.Wait();
  }
  io_thread_ = nullptr;
  gpu_thread_ = nullptr;
}

void PixelTest::EnableExternalStencilTest() {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_has_external_stencil_test(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  output_surface_.reset(new PixelTestOutputSurface(
      std::make_unique<viz::SoftwareOutputDevice>()));
  output_surface_->BindToClient(output_surface_client_.get());
  shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
  resource_provider_ = std::make_unique<viz::DisplayResourceProvider>(
      viz::DisplayResourceProvider::kSoftware, nullptr,
      shared_bitmap_manager_.get());
  child_resource_provider_ =
      std::make_unique<viz::ClientResourceProvider>(true);

  auto renderer = std::make_unique<viz::SoftwareRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get());
  software_renderer_ = renderer.get();
  renderer_ = std::move(renderer);
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

}  // namespace cc

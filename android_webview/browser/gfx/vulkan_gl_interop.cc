// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/vulkan_gl_interop.h"

#include <utility>

#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"
#include "third_party/skia/include/private/chromium/GrVkSecondaryCBDrawContext.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/android/egl_fence_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {

void CleanupInFlightDraw(sk_sp<GrVkSecondaryCBDrawContext> draw_context,
                         VkSemaphore post_draw_semaphore,
                         gpu::VulkanDeviceQueue* device_queue,
                         bool /* context_lost */) {
  VkDevice device = device_queue->GetVulkanDevice();
  // We do the same thing whether or not context is lost.
  draw_context->releaseResources();
  draw_context.reset();
  if (post_draw_semaphore != VK_NULL_HANDLE)
    vkDestroySemaphore(device, post_draw_semaphore, nullptr);
}
}  // namespace

// static
VulkanGLInterop::GLNonOwnedCompatibilityContext* VulkanGLInterop::g_gl_context =
    nullptr;

class VulkanGLInterop::GLNonOwnedCompatibilityContext
    : public gl::GLContextEGL {
 public:
  GLNonOwnedCompatibilityContext()
      : gl::GLContextEGL(nullptr),
        surface_(base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(
            gl::GLSurfaceEGL::GetGLDisplayEGL(),
            gfx::Size(1, 1))) {
    gl::GLContextAttribs attribs;
    Initialize(surface_.get(), attribs);

    DCHECK(!g_gl_context);
    g_gl_context = this;
  }

  GLNonOwnedCompatibilityContext(const GLNonOwnedCompatibilityContext&) =
      delete;
  GLNonOwnedCompatibilityContext& operator=(
      const GLNonOwnedCompatibilityContext&) = delete;

  bool MakeCurrentImpl(gl::GLSurface* surface) override {
    // A GLNonOwnedCompatibilityContext may have set the GetRealCurrent()
    // pointer to itself, while re-using our EGL context. In these cases just
    // call SetCurrent to restore expected GetRealContext().
    if (GetHandle() == eglGetCurrentContext()) {
      if (surface) {
        // No one should change the current EGL surface without also changing
        // the context.
        DCHECK(eglGetCurrentSurface(EGL_DRAW) == surface->GetHandle());
      }

      SetCurrent(surface);
      return true;
    }

    return gl::GLContextEGL::MakeCurrentImpl(surface);
  }

  bool MakeCurrent() { return gl::GLContext::MakeCurrent(surface_.get()); }

  static scoped_refptr<GLNonOwnedCompatibilityContext> GetOrCreateInstance() {
    if (g_gl_context)
      return base::WrapRefCounted(g_gl_context);
    return base::WrapRefCounted(new GLNonOwnedCompatibilityContext);
  }

 private:
  ~GLNonOwnedCompatibilityContext() override {
    DCHECK_EQ(g_gl_context, this);
    g_gl_context = nullptr;
  }

  scoped_refptr<gl::GLSurface> surface_;
};

VulkanGLInterop::InFlightInteropDraw::InFlightInteropDraw(
    AwVulkanContextProvider* vk_context_provider)
    : vk_context_provider(vk_context_provider) {}

VulkanGLInterop::InFlightInteropDraw::~InFlightInteropDraw() {
  // If |draw_context| is valid, we encountered an error during Vk drawing and
  // should call vkQueueWaitIdle to ensure safe shutdown.
  bool encountered_error = !!draw_context;
  if (encountered_error) {
    // Clean up one-off objects which may have been left alive due to an error.

    // Clean up |ahb_skimage| first, as doing so generates Vk commands we need
    // to flush before the vkQueueWaitIdle below.
    if (ahb_skimage) {
      ahb_skimage.reset();
      vk_context_provider->GetGrContext()->flushAndSubmit();
    }
    // We encountered an error and are not sure when our Vk objects are safe to
    // delete. VkQueueWaitIdle to ensure safety.
    vkQueueWaitIdle(vk_context_provider->queue());
    if (draw_context) {
      draw_context->releaseResources();
      draw_context.reset();
    }
    if (post_draw_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(vk_context_provider->device(), post_draw_semaphore,
                         nullptr);
      post_draw_semaphore = VK_NULL_HANDLE;
    }
  }
  DCHECK(!draw_context);
  DCHECK(!ahb_skimage);
  DCHECK(post_draw_semaphore == VK_NULL_HANDLE);

  // Clean up re-usable components that are expected to still be alive.
  if (texture_id)
    glDeleteTextures(1, &texture_id);
  if (framebuffer_id)
    glDeleteFramebuffersEXT(1, &framebuffer_id);
  if (vulkan_image) {
    vk_context_provider->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(vulkan_image));
  }
}

VulkanGLInterop::VulkanGLInterop(
    RenderThreadManager* render_thread_manager,
    AwVulkanContextProvider* vulkan_context_provider)
    : render_thread_manager_(render_thread_manager),
      vulkan_context_provider_(vulkan_context_provider),
      gl_context_(GLNonOwnedCompatibilityContext::GetOrCreateInstance()) {
  DCHECK(render_thread_manager_);
  DCHECK(vulkan_context_provider_);
}

VulkanGLInterop::~VulkanGLInterop() {
  // Clear the queue.
  { auto queue = std::move(in_flight_interop_draws_); }

  gl_context_.reset();
}

void VulkanGLInterop::DrawVk(sk_sp<GrVkSecondaryCBDrawContext> draw_context,
                             sk_sp<SkColorSpace> color_space,
                             const HardwareRendererDrawParams& params,
                             const OverlaysParams& overlays_params) {
  if (!gl_context_)
    return;

  if (!gl_context_->MakeCurrent()) {
    LOG(ERROR) << "Failed to make GL context current for drawing.";
    return;
  }

  // If |pending_draw_| is non-null, we called DrawVk twice without PostDrawVk.
  DCHECK(!pending_draw_);

  // Use a temporary to automatically clean up if we hit a failure case.
  std::unique_ptr<InFlightInteropDraw> pending_draw;

  // If we've exhausted our buffers, re-use an existing one.
  // TODO(ericrk): Benchmark using more than 1 buffer.
  if (in_flight_interop_draws_.size() >= 1 /* single buffering */) {
    pending_draw = std::move(in_flight_interop_draws_.front());
    in_flight_interop_draws_.pop();
  }

  // If prev buffer is wrong size, just re-allocate.
  if (pending_draw &&
      pending_draw->image_size != gfx::Size(params.width, params.height)) {
    pending_draw.reset();
  }

  // If we weren't able to re-use a previous draw, create one.
  if (!pending_draw) {
    pending_draw =
        std::make_unique<InFlightInteropDraw>(vulkan_context_provider_);

    AHardwareBuffer_Desc desc = {};
    desc.width = params.width;
    desc.height = params.height;
    desc.layers = 1;  // number of images
    // TODO(ericrk): Handle other formats.
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_NEVER |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER |
                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                 AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    AHardwareBuffer* buffer = nullptr;
    base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
    if (!buffer) {
      LOG(ERROR) << "Failed to allocate AHardwareBuffer for WebView rendering.";
      return;
    }
    pending_draw->scoped_buffer =
        base::android::ScopedHardwareBufferHandle::Adopt(buffer);
    pending_draw->image_size = gfx::Size(params.width, params.height);

    // Create an EGLImage for the buffer.
    pending_draw->egl_image = gpu::CreateEGLImageFromAHardwareBuffer(
        pending_draw->scoped_buffer.get());
    if (!pending_draw->egl_image.is_valid()) {
      LOG(ERROR) << "Failed to initialize EGLImage for AHardwareBuffer";
      return;
    }

    glGenTextures(1, static_cast<GLuint*>(&pending_draw->texture_id));
    GLenum target = GL_TEXTURE_2D;
    glBindTexture(target, pending_draw->texture_id);
    glEGLImageTargetTexture2DOES(target, pending_draw->egl_image.get());
    glBindTexture(target, 0);
    glGenFramebuffersEXT(1, &pending_draw->framebuffer_id);
    glBindFramebufferEXT(GL_FRAMEBUFFER, pending_draw->framebuffer_id);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, pending_draw->texture_id, 0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "Failed to set up framebuffer for WebView GL drawing.";
      return;
    }
  }

  // Ask GL to wait on any Vk sync_fd before writing.
  gl::InsertEglFenceAndWait(std::move(pending_draw->sync_fd));

  // Bind buffer and render with GL.
  base::ScopedFD gl_done_fd;
  {
    glBindFramebufferEXT(GL_FRAMEBUFFER, pending_draw->framebuffer_id);
    glViewport(0, 0, params.width, params.height);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    render_thread_manager_->DrawOnRT(/*save_restore=*/false, params,
                                     overlays_params);
    gl_done_fd = gl::CreateEglFenceAndExportFd();
  }

  pending_draw->draw_context = std::move(draw_context);

  // If we have a |gl_done_fd|, create a Skia GrBackendSemaphore from
  // |gl_done_fd| and wait.
  if (gl_done_fd.is_valid()) {
    VkSemaphore gl_done_semaphore =
        vulkan_context_provider_->GetVulkanImplementation()
            ->ImportSemaphoreHandle(
                vulkan_context_provider_->device(),
                gpu::SemaphoreHandle(
                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                    std::move(gl_done_fd)));
    if (gl_done_semaphore == VK_NULL_HANDLE) {
      LOG(ERROR) << "Could not create Vulkan semaphore for GL completion.";
      return;
    }
    GrBackendSemaphore gr_semaphore;
    gr_semaphore.initVulkan(gl_done_semaphore);
    if (!pending_draw->draw_context->wait(1, &gr_semaphore)) {
      // If wait returns false, we must clean up the |gl_done_semaphore|.
      vkDestroySemaphore(vulkan_context_provider_->device(), gl_done_semaphore,
                         nullptr);
      LOG(ERROR) << "Could not wait on GL completion semaphore.";
      return;
    }
  }

  // Create a VkImage and import AHB.
  if (!pending_draw->vulkan_image) {
    auto handle = base::android::ScopedHardwareBufferHandle::Create(
        pending_draw->scoped_buffer.get());
    gfx::GpuMemoryBufferHandle gmb_handle(std::move(handle));
    auto* device_queue = vulkan_context_provider_->GetDeviceQueue();
    auto vulkan_image = gpu::VulkanImage::CreateFromGpuMemoryBufferHandle(
        device_queue, std::move(gmb_handle),
        gfx::Size(params.width, params.height), VK_FORMAT_R8G8B8A8_UNORM,
        /*usage=*/0, /*flags=*/0, /*image_tiling=*/VK_IMAGE_TILING_OPTIMAL,
        /*queue_family_index=*/VK_QUEUE_FAMILY_EXTERNAL);
    if (!vulkan_image) {
      LOG(ERROR) << "Could not create VkImage from AHB.";
      return;
    }

    pending_draw->image_info = gpu::CreateGrVkImageInfo(vulkan_image.get());
    pending_draw->vulkan_image = std::move(vulkan_image);
  }

  // Create an SkImage from AHB.
  GrBackendTexture backend_texture(params.width, params.height,
                                   pending_draw->image_info);
  pending_draw->ahb_skimage = SkImages::BorrowTextureFrom(
      vulkan_context_provider_->GetGrContext(), backend_texture,
      kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      color_space);
  if (!pending_draw->ahb_skimage) {
    LOG(ERROR) << "Could not create SkImage from VkImage.";
    return;
  }

  // Draw the SkImage.
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrcOver);
  pending_draw->draw_context->getCanvas()->drawImage(
      pending_draw->ahb_skimage, 0, 0, SkSamplingOptions(), &paint);
  pending_draw->draw_context->flush();

  // Transfer |pending_draw| to |pending_draw_| for handling in
  // PostDrawVkInterop.
  pending_draw_ = std::move(pending_draw);
}

void VulkanGLInterop::PostDrawVk() {
  // Use a temporary to automatically clean up if we hit a failure case.
  std::unique_ptr<InFlightInteropDraw> pending_draw = std::move(pending_draw_);

  if (!gl_context_ || !pending_draw)
    return;

  // Get the final state of the SkImage so that we can pass this back to Skia
  // during re-use.
  GrBackendTexture backend_texture;
  if (!SkImages::GetBackendTextureFromImage(
          pending_draw->ahb_skimage, &backend_texture,
          true /* flushPendingGrContextIO */)) {
    LOG(ERROR) << "Could not get Vk backend texture.";
    return;
  }
  GrVkImageInfo image_info;
  if (!backend_texture.getVkImageInfo(&image_info)) {
    LOG(ERROR) << "Could not get Vk image info.";
    return;
  }

  // Copy image layout to our cached image info.
  pending_draw->image_info.fImageLayout = image_info.fImageLayout;

  // Release the SkImage so that Skia transitions it back to
  // VK_QUEUE_FAMILY_EXTERNAL.
  pending_draw->ahb_skimage.reset();

  // Create a semaphore to track the image's transition back to external.
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;
  VkResult result =
      vkCreateSemaphore(vulkan_context_provider_->device(), &sem_info, nullptr,
                        &pending_draw->post_draw_semaphore);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Could not create VkSemaphore.";
    return;
  }
  GrBackendSemaphore gr_post_draw_semaphore;
  gr_post_draw_semaphore.initVulkan(pending_draw->post_draw_semaphore);

  // Flush so that we know the image's transition has been submitted and that
  // the |post_draw_semaphore| is pending.
  GrFlushInfo flushInfo;
  flushInfo.fNumSemaphores = 1;
  flushInfo.fSignalSemaphores = &gr_post_draw_semaphore;
  GrSemaphoresSubmitted submitted =
      vulkan_context_provider_->GetGrContext()->flush(flushInfo);
  vulkan_context_provider_->GetGrContext()->submit();
  if (submitted != GrSemaphoresSubmitted::kYes) {
    LOG(ERROR) << "Skia could not submit GrSemaphore.";
    return;
  }
  gpu::SemaphoreHandle semaphore_handle =
      vulkan_context_provider_->GetVulkanImplementation()->GetSemaphoreHandle(
          vulkan_context_provider_->device(),
          pending_draw->post_draw_semaphore);
  if (!semaphore_handle.is_valid()) {
    LOG(ERROR) << "Could not retrieve SyncFD from |post_draw_semaphore|.";
    return;
  }
  pending_draw->sync_fd = semaphore_handle.TakeHandle();

  gpu::VulkanFenceHelper* fence_helper =
      vulkan_context_provider_->GetDeviceQueue()->GetFenceHelper();

  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      &CleanupInFlightDraw, std::move(pending_draw->draw_context),
      pending_draw->post_draw_semaphore));
  pending_draw->post_draw_semaphore = VK_NULL_HANDLE;

  // Add the |pending_draw| to |in_flight_interop_draws_|.
  in_flight_interop_draws_.push(std::move(pending_draw));

  // Process cleanup tasks and generate fences at the end of each PostDrawVk.
  // TODO(ericrk): We'd ideally combine this with the flushAndSignalSemaphores
  // above.
  fence_helper->GenerateCleanupFence();
  fence_helper->ProcessCleanupTasks();
}

void VulkanGLInterop::MakeGLContextCurrentIgnoreFailure() {
  if (gl_context_)
    gl_context_->MakeCurrent();
}

}  // namespace android_webview

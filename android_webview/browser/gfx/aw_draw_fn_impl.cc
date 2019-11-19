// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_draw_fn_impl.h"

#include <utility>

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser_jni_headers/AwDrawFnImpl_jni.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

using base::android::JavaParamRef;
using content::BrowserThread;

namespace android_webview {

namespace {
GLNonOwnedCompatibilityContext* g_gl_context = nullptr;

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
}

class GLNonOwnedCompatibilityContext : public gl::GLContextEGL {
 public:
  GLNonOwnedCompatibilityContext()
      : gl::GLContextEGL(nullptr),
        surface_(
            base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(gfx::Size(1, 1))) {
    gl::GLContextAttribs attribs;
    Initialize(surface_.get(), attribs);

    DCHECK(!g_gl_context);
    g_gl_context = this;
  }

  bool MakeCurrent(gl::GLSurface* surface) override {
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

    return gl::GLContextEGL::MakeCurrent(surface);
  }

  bool MakeCurrent() { return MakeCurrent(surface_.get()); }

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

  DISALLOW_COPY_AND_ASSIGN(GLNonOwnedCompatibilityContext);
};

namespace {

AwDrawFnFunctionTable* g_draw_fn_function_table = nullptr;

void OnSyncWrapper(int functor, void* data, AwDrawFn_OnSyncParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnSync", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnSync(params);
}

void OnContextDestroyedWrapper(int functor, void* data) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnContextDestroyed",
               "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnContextDestroyed();
}

void OnDestroyedWrapper(int functor, void* data) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnDestroyed", "functor",
               functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  delete static_cast<AwDrawFnImpl*>(data);
}

void DrawGLWrapper(int functor, void* data, AwDrawFn_DrawGLParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_DrawGL", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawGL(params);
}

void InitVkWrapper(int functor, void* data, AwDrawFn_InitVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_InitVk", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->InitVk(params);
}

void DrawVkWrapper(int functor, void* data, AwDrawFn_DrawVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_DrawVk", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawVk(params);
}

void PostDrawVkWrapper(int functor,
                       void* data,
                       AwDrawFn_PostDrawVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_PostDrawVk", "functor",
               functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->PostDrawVk(params);
}

sk_sp<GrVkSecondaryCBDrawContext> CreateDrawContext(
    GrContext* gr_context,
    AwDrawFn_DrawVkParams* params,
    sk_sp<SkColorSpace> color_space) {
  // Create a GrVkSecondaryCBDrawContext to render our AHB w/ Vulkan.
  // TODO(ericrk): Handle non-RGBA.
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params->width, params->height, color_space);
  VkRect2D draw_bounds;
  GrVkDrawableInfo drawable_info{
      .fSecondaryCommandBuffer = params->secondary_command_buffer,
      .fColorAttachmentIndex = params->color_attachment_index,
      .fCompatibleRenderPass = params->compatible_render_pass,
      .fFormat = params->format,
      .fDrawBounds = &draw_bounds,
  };
  SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
  return GrVkSecondaryCBDrawContext::Make(gr_context, info, drawable_info,
                                          &props);
}

template <typename T>
sk_sp<SkColorSpace> CreateColorSpace(T* params) {
  skcms_TransferFunction transfer_fn{
      params->transfer_function_g, params->transfer_function_a,
      params->transfer_function_b, params->transfer_function_c,
      params->transfer_function_d, params->transfer_function_e,
      params->transfer_function_f};
  skcms_Matrix3x3 to_xyz;
  static_assert(sizeof(to_xyz.vals) == sizeof(params->color_space_toXYZD50),
                "Color space matrix sizes do not match");
  memcpy(&to_xyz.vals[0][0], &params->color_space_toXYZD50[0],
         sizeof(to_xyz.vals));
  return SkColorSpace::MakeRGB(transfer_fn, to_xyz);
}

}  // namespace

static void JNI_AwDrawFnImpl_SetDrawFnFunctionTable(JNIEnv* env,
                                                    jlong function_table) {
  g_draw_fn_function_table =
      reinterpret_cast<AwDrawFnFunctionTable*>(function_table);
}

AwDrawFnImpl::AwDrawFnImpl()
    : is_interop_mode_(!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewEnableVulkan)),
      render_thread_manager_(
          base::CreateSingleThreadTaskRunner({BrowserThread::UI})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_draw_fn_function_table);

  static AwDrawFnFunctorCallbacks g_functor_callbacks{
      &OnSyncWrapper,      &OnContextDestroyedWrapper,
      &OnDestroyedWrapper, &DrawGLWrapper,
      &InitVkWrapper,      &DrawVkWrapper,
      &PostDrawVkWrapper,
  };

  functor_handle_ =
      g_draw_fn_function_table->create_functor(this, &g_functor_callbacks);
}

AwDrawFnImpl::~AwDrawFnImpl() {}

void AwDrawFnImpl::ReleaseHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  render_thread_manager_.RemoveFromCompositorFrameProducerOnUI();
  g_draw_fn_function_table->release_functor(functor_handle_);
}

jint AwDrawFnImpl::GetFunctorHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return functor_handle_;
}

jlong AwDrawFnImpl::GetCompositorFrameConsumer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(GetCompositorFrameConsumer());
}

static jlong JNI_AwDrawFnImpl_Create(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(new AwDrawFnImpl());
}

void AwDrawFnImpl::OnSync(AwDrawFn_OnSyncParams* params) {
  render_thread_manager_.UpdateViewTreeForceDarkStateOnRT(
      params->apply_force_dark);
  render_thread_manager_.CommitFrameOnRT();
}

void AwDrawFnImpl::OnContextDestroyed() {
  // Try to make GL current to safely clean up. Ignore failures.
  if (gl_context_)
    gl_context_->MakeCurrent();

  {
    RenderThreadManager::InsideHardwareReleaseReset release_reset(
        &render_thread_manager_);
    render_thread_manager_.DestroyHardwareRendererOnRT(
        false /* save_restore */);
  }

  // Clear the queue.
  { auto queue = std::move(in_flight_interop_draws_); }

  vulkan_context_provider_.reset();
  gl_context_.reset();
}

void AwDrawFnImpl::DrawGL(AwDrawFn_DrawGLParams* params) {
  auto color_space = params->version >= 2 ? CreateColorSpace(params) : nullptr;
  DrawInternal(params, color_space.get());
}

void AwDrawFnImpl::InitVk(AwDrawFn_InitVkParams* params) {
  // We should never have a |vulkan_context_provider_| if we are calling VkInit.
  // This means context destroyed was not correctly called.
  DCHECK(!vulkan_context_provider_);
  vulkan_context_provider_ =
      AwVulkanContextProvider::GetOrCreateInstance(params);

  // Make sure we have a GL context.
  DCHECK(!gl_context_);
  gl_context_ = GLNonOwnedCompatibilityContext::GetOrCreateInstance();
}

void AwDrawFnImpl::DrawVk(AwDrawFn_DrawVkParams* params) {
  if (is_interop_mode_) {
    DrawVkInterop(params);
  } else {
    DrawVkDirect(params);
  }
}

void AwDrawFnImpl::PostDrawVk(AwDrawFn_PostDrawVkParams* params) {
  if (is_interop_mode_) {
    PostDrawVkInterop(params);
  } else {
    PostDrawVkDirect(params);
  }
}

void AwDrawFnImpl::DrawVkDirect(AwDrawFn_DrawVkParams* params) {
  if (!vulkan_context_provider_)
    return;

  DCHECK(!scoped_secondary_cb_draw_);

  auto color_space = CreateColorSpace(params);
  if (!color_space) {
    // If we weren't passed a valid colorspace, default to sRGB.
    LOG(ERROR) << "Received invalid colorspace.";
    color_space = SkColorSpace::MakeSRGB();
  }
  auto draw_context = CreateDrawContext(vulkan_context_provider_->gr_context(),
                                        params, color_space);

  // Set the draw contexct in |vulkan_context_provider_|, so the SkiaRenderer
  // and SkiaOutputSurface* will use it as frame render target.
  scoped_secondary_cb_draw_.emplace(vulkan_context_provider_.get(),
                                    std::move(draw_context));
  DrawInternal(params, color_space.get());
}

void AwDrawFnImpl::PostDrawVkDirect(AwDrawFn_PostDrawVkParams* params) {
  if (!vulkan_context_provider_)
    return;

  DCHECK(scoped_secondary_cb_draw_);
  scoped_secondary_cb_draw_.reset();
}

void AwDrawFnImpl::DrawVkInterop(AwDrawFn_DrawVkParams* params) {
  if (!vulkan_context_provider_ || !gl_context_)
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
  if (pending_draw && pending_draw->ahb_image->GetSize() !=
                          gfx::Size(params->width, params->height)) {
    pending_draw.reset();
  }

  // If we weren't able to re-use a previous draw, create one.
  if (!pending_draw) {
    pending_draw =
        std::make_unique<InFlightInteropDraw>(vulkan_context_provider_.get());

    AHardwareBuffer_Desc desc = {};
    desc.width = params->width;
    desc.height = params->height;
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
    auto scoped_buffer =
        base::android::ScopedHardwareBufferHandle::Adopt(buffer);

    pending_draw->ahb_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(
        gfx::Size(params->width, params->height));
    if (!pending_draw->ahb_image->Initialize(scoped_buffer.get(),
                                             false /* preserved */)) {
      LOG(ERROR) << "Failed to initialize GLImage for AHardwareBuffer.";
      return;
    }

    glGenTextures(1, static_cast<GLuint*>(&pending_draw->texture_id));
    GLenum target = GL_TEXTURE_2D;
    glBindTexture(target, pending_draw->texture_id);
    if (!pending_draw->ahb_image->BindTexImage(target)) {
      LOG(ERROR) << "Failed to bind GLImage for AHardwareBuffer.";
      return;
    }
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
  gpu::InsertEglFenceAndWait(std::move(pending_draw->sync_fd));

  auto color_space = CreateColorSpace(params);
  if (!color_space) {
    // If we weren't passed a valid colorspace, default to sRGB.
    LOG(ERROR) << "Received invalid colorspace.";
    color_space = SkColorSpace::MakeSRGB();
  }

  // Bind buffer and render with GL.
  base::ScopedFD gl_done_fd;
  {
    glBindFramebufferEXT(GL_FRAMEBUFFER, pending_draw->framebuffer_id);
    glViewport(0, 0, params->width, params->height);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    DrawInternal(params, color_space.get());
    gl_done_fd = gpu::CreateEglFenceAndExportFd();
  }

  pending_draw->draw_context = CreateDrawContext(
      vulkan_context_provider_->gr_context(), params, color_space);

  // If we have a |gl_done_fd|, create a Skia GrBackendSemaphore from
  // |gl_done_fd| and wait.
  if (gl_done_fd.is_valid()) {
    VkSemaphore gl_done_semaphore =
        vulkan_context_provider_->implementation()->ImportSemaphoreHandle(
            vulkan_context_provider_->device(),
            gpu::SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
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
  if (!pending_draw->image_info.fImage) {
    VkImage vk_image;
    VkImageCreateInfo vk_image_info;
    VkDeviceMemory vk_device_memory;
    VkDeviceSize mem_allocation_size;
    if (!vulkan_context_provider_->implementation()->CreateVkImageAndImportAHB(
            vulkan_context_provider_->device(),
            vulkan_context_provider_->physical_device(),
            gfx::Size(params->width, params->height),
            base::android::ScopedHardwareBufferHandle::Create(
                pending_draw->ahb_image->GetAHardwareBuffer()->buffer()),
            &vk_image, &vk_image_info, &vk_device_memory,
            &mem_allocation_size)) {
      LOG(ERROR) << "Could not create VkImage from AHB.";
      return;
    }

    // Create backend texture from the VkImage.
    GrVkAlloc alloc = {vk_device_memory, 0, mem_allocation_size, 0};
    pending_draw->image_info = {vk_image,
                                alloc,
                                vk_image_info.tiling,
                                vk_image_info.initialLayout,
                                vk_image_info.format,
                                vk_image_info.mipLevels,
                                VK_QUEUE_FAMILY_EXTERNAL};
  }

  // Create an SkImage from AHB.
  GrBackendTexture backend_texture(params->width, params->height,
                                   pending_draw->image_info);
  pending_draw->ahb_skimage = SkImage::MakeFromTexture(
      vulkan_context_provider_->gr_context(), backend_texture,
      kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      color_space);
  if (!pending_draw->ahb_skimage) {
    LOG(ERROR) << "Could not create SkImage from VkImage.";
    return;
  }

  // Draw the SkImage.
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrcOver);
  pending_draw->draw_context->getCanvas()->drawImage(pending_draw->ahb_skimage,
                                                     0, 0, &paint);
  pending_draw->draw_context->flush();

  // Transfer |pending_draw| to |pending_draw_| for handling in
  // PostDrawVkInterop.
  pending_draw_ = std::move(pending_draw);
}

void AwDrawFnImpl::PostDrawVkInterop(AwDrawFn_PostDrawVkParams* params) {
  // Use a temporary to automatically clean up if we hit a failure case.
  std::unique_ptr<InFlightInteropDraw> pending_draw = std::move(pending_draw_);

  if (!vulkan_context_provider_ || !gl_context_ || !pending_draw)
    return;

  // Get the final state of the SkImage so that we can pass this back to Skia
  // during re-use.
  GrBackendTexture backend_texture =
      pending_draw->ahb_skimage->getBackendTexture(
          true /* flushPendingGrContextIO */);
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
  GrSemaphoresSubmitted submitted =
      vulkan_context_provider_->gr_context()->flushAndSignalSemaphores(
          1, &gr_post_draw_semaphore);
  if (submitted != GrSemaphoresSubmitted::kYes) {
    LOG(ERROR) << "Skia could not submit GrSemaphore.";
    return;
  }
  gpu::SemaphoreHandle semaphore_handle =
      vulkan_context_provider_->implementation()->GetSemaphoreHandle(
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

template <typename T>
void AwDrawFnImpl::DrawInternal(T* params, SkColorSpace* color_space) {
  struct HardwareRendererDrawParams hr_params {};
  hr_params.clip_left = params->clip_left;
  hr_params.clip_top = params->clip_top;
  hr_params.clip_right = params->clip_right;
  hr_params.clip_bottom = params->clip_bottom;
  hr_params.width = params->width;
  hr_params.height = params->height;
  if (color_space)
    hr_params.color_space = gfx::ColorSpace(*color_space);

  static_assert(base::size(decltype(params->transform){}) ==
                    base::size(hr_params.transform),
                "transform size mismatch");
  for (size_t i = 0; i < base::size(hr_params.transform); ++i) {
    hr_params.transform[i] = params->transform[i];
  }
  render_thread_manager_.DrawOnRT(false /* save_restore */, &hr_params);
}

AwDrawFnImpl::InFlightInteropDraw::InFlightInteropDraw(
    AwVulkanContextProvider* vk_context_provider)
    : vk_context_provider(vk_context_provider) {}

AwDrawFnImpl::InFlightInteropDraw::~InFlightInteropDraw() {
  // If |draw_context| is valid, we encountered an error during Vk drawing and
  // should call vkQueueWaitIdle to ensure safe shutdown.
  bool encountered_error = !!draw_context;
  if (encountered_error) {
    // Clean up one-off objects which may have been left alive due to an error.

    // Clean up |ahb_skimage| first, as doing so generates Vk commands we need
    // to flush before the vkQueueWaitIdle below.
    if (ahb_skimage) {
      ahb_skimage.reset();
      vk_context_provider->gr_context()->flush();
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
  if (image_info.fImage != VK_NULL_HANDLE) {
    vk_context_provider->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueImageCleanupForSubmittedWork(image_info.fImage,
                                              image_info.fAlloc.fMemory);
  }
}

}  // namespace android_webview

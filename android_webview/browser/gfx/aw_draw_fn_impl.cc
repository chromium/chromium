// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_draw_fn_impl.h"

#include <sys/prctl.h>

#include <utility>

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "base/android/build_info.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"
#include "third_party/skia/include/private/chromium/GrVkSecondaryCBDrawContext.h"
#include "ui/gfx/color_space.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwDrawFnImpl_jni.h"

using base::android::JavaParamRef;
using content::BrowserThread;

namespace android_webview {

namespace {

BASE_FEATURE(kCheckDrawFunctorThread,
             "CheckDrawFunctorThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set once during process-wide initialization.
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

void RemoveOverlaysWrapper(int functor,
                           void* data,
                           AwDrawFn_RemoveOverlaysParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_RemoveOverlays", "functor",
               functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->RemoveOverlays(params);
}

sk_sp<GrVkSecondaryCBDrawContext> CreateDrawContext(
    GrDirectContext* gr_context,
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
  SkSurfaceProps props{0, kUnknown_SkPixelGeometry};
  sk_sp<GrVkSecondaryCBDrawContext> context =
      GrVkSecondaryCBDrawContext::Make(gr_context, info, drawable_info, &props);
  LOG_IF(FATAL, !context)
      << "Failed GrVkSecondaryCBDrawContext::Make"
      << " fSecondaryCommandBuffer:" << params->secondary_command_buffer
      << " fColorAttachmentIndex:" << params->color_attachment_index
      << " fCompatibleRenderPass:" << params->compatible_render_pass
      << " fFormat:" << params->format << " width:" << params->width
      << " height:" << params->height
      << " is_srgb:" << (color_space == SkColorSpace::MakeSRGB());
  return context;
}

OverlaysParams::Mode GetOverlaysMode(AwDrawFnOverlaysMode mode) {
  switch (mode) {
    case AW_DRAW_FN_OVERLAYS_MODE_DISABLED:
      return OverlaysParams::Mode::Disabled;
    case AW_DRAW_FN_OVERLAYS_MODE_ENABLED:
      return OverlaysParams::Mode::Enabled;
    default:
      NOTREACHED();
  }
}

template <typename T>
HardwareRendererDrawParams CreateHRDrawParams(T* params,
                                              SkColorSpace* color_space) {
  struct HardwareRendererDrawParams hr_params {};
  hr_params.clip_left = params->clip_left;
  hr_params.clip_top = params->clip_top;
  hr_params.clip_right = params->clip_right;
  hr_params.clip_bottom = params->clip_bottom;
  hr_params.width = params->width;
  hr_params.height = params->height;
  if (color_space)
    hr_params.color_space = gfx::ColorSpace(*color_space);

  static_assert(std::size(decltype(params->transform){}) ==
                    std::size(hr_params.transform),
                "transform size mismatch");
  for (size_t i = 0; i < std::size(hr_params.transform); ++i) {
    hr_params.transform[i] = params->transform[i];
  }

  return hr_params;
}

template <class T>
OverlaysParams CreateOverlaysParams(T* draw_params) {
  OverlaysParams params;
  if (draw_params->version >= 3) {
    params.overlays_mode = GetOverlaysMode(draw_params->overlays_mode);
    params.get_surface_control = draw_params->get_surface_control;
    params.merge_transaction = draw_params->merge_transaction;
  }
  return params;
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

// static
bool AwDrawFnImpl::IsUsingVulkan() {
  return g_draw_fn_function_table &&
         g_draw_fn_function_table->query_render_mode() ==
             AW_DRAW_FN_RENDER_MODE_VULKAN;
}

// static
void AwDrawFnImpl::ReportRenderingThreads(int functor,
                                          const pid_t* thread_ids,
                                          size_t size) {
  if (g_draw_fn_function_table && g_draw_fn_function_table->version >= 4) {
    g_draw_fn_function_table->report_rendering_threads(functor, thread_ids,
                                                       size);
  }
}

AwDrawFnImpl::AwDrawFnImpl()
    : render_thread_manager_(content::GetUIThreadTaskRunner({})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_draw_fn_function_table);

  static AwDrawFnFunctorCallbacks g_functor_callbacks{
      &OnSyncWrapper,      &OnContextDestroyedWrapper,
      &OnDestroyedWrapper, &DrawGLWrapper,
      &InitVkWrapper,      &DrawVkWrapper,
      &PostDrawVkWrapper,  &RemoveOverlaysWrapper};

  if (g_draw_fn_function_table->version >= 3) {
    functor_handle_ = g_draw_fn_function_table->create_functor_v3(
        this, kAwDrawFnVersion, &g_functor_callbacks);
  } else {
    functor_handle_ =
        g_draw_fn_function_table->create_functor(this, &g_functor_callbacks);
  }
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
  if (render_thread_id_) {
    auto current_id = base::PlatformThread::CurrentId();
    if (render_thread_id_.value() != current_id) {
      constexpr size_t kBufferLen = 64;
      char name[kBufferLen] = {};
      int err = prctl(PR_GET_NAME, name);

      if (!err) {
        LOG(FATAL) << "OnContextDestroyed called on: " << current_id << "/"
                   << name << " rt: " << render_thread_id_.value();
      } else {
        LOG(FATAL) << "OnContextDestroyed called on: " << current_id
                   << " rt: " << render_thread_id_.value();
      }
    }
  }

  {
    RenderThreadManager::InsideHardwareReleaseReset release_reset(
        &render_thread_manager_);
    render_thread_manager_.DestroyHardwareRendererOnRT(
        false /* save_restore */, false /* abandon_context */);
  }

  vulkan_context_provider_.reset();
}

void AwDrawFnImpl::DrawGL(AwDrawFn_DrawGLParams* params) {
  if (!render_thread_id_ &&
      base::FeatureList::IsEnabled(kCheckDrawFunctorThread)) {
    render_thread_id_ = base::PlatformThread::CurrentId();
  }

  auto color_space = params->version >= 2 ? CreateColorSpace(params) : nullptr;
  HardwareRendererDrawParams hr_params =
      CreateHRDrawParams(params, color_space.get());
  OverlaysParams overlays_params = CreateOverlaysParams(params);
  render_thread_manager_.DrawOnRT(
      /*save_restore=*/false, hr_params, overlays_params,
      base::BindOnce(&AwDrawFnImpl::ReportRenderingThreads, functor_handle_));
}

void AwDrawFnImpl::InitVk(AwDrawFn_InitVkParams* params) {
  if (!render_thread_id_ &&
      base::FeatureList::IsEnabled(kCheckDrawFunctorThread)) {
    render_thread_id_ = base::PlatformThread::CurrentId();
  }

  // We should never have a |vulkan_context_provider_| if we are calling VkInit.
  // This means context destroyed was not correctly called.
  DCHECK(!vulkan_context_provider_);
  vulkan_context_provider_ = AwVulkanContextProvider::Create(params);
  DCHECK(vulkan_context_provider_);

  render_thread_manager_.SetVulkanContextProviderOnRT(
      vulkan_context_provider_.get());
}

void AwDrawFnImpl::DrawVk(AwDrawFn_DrawVkParams* params) {
  if (!vulkan_context_provider_)
    return;

  // Android HWUI has a bug that asks functor to draw into a 8-bit mask for
  // functionality that is not related to and not needed by webview.
  // GrVkSecondaryCBDrawContext currently does not expect or support R8 format
  // so just skip these draw calls before Android side is fixed.
  if (params->format == VK_FORMAT_R8_UNORM &&
      base::android::BuildInfo::GetInstance()->sdk_int() ==
          base::android::SDK_VERSION_S) {
    skip_next_post_draw_vk_ = true;
    return;
  }

  auto color_space = CreateColorSpace(params);
  if (!color_space) {
    // If we weren't passed a valid colorspace, default to sRGB.
    LOG(ERROR) << "Received invalid colorspace.";
    color_space = SkColorSpace::MakeSRGB();
  }
  auto draw_context = CreateDrawContext(
      vulkan_context_provider_->GetGrContext(), params, color_space);
  HardwareRendererDrawParams hr_params =
      CreateHRDrawParams(params, color_space.get());
  OverlaysParams overlays_params = CreateOverlaysParams(params);

  DCHECK(!scoped_secondary_cb_draw_);

  // Set the draw contexct in |vulkan_context_provider_|, so the SkiaRenderer
  // and SkiaOutputSurface* will use it as frame render target.
  scoped_secondary_cb_draw_.emplace(vulkan_context_provider_.get(),
                                    std::move(draw_context));
  render_thread_manager_.DrawOnRT(
      false /* save_restore */, hr_params, overlays_params,
      base::BindOnce(&AwDrawFnImpl::ReportRenderingThreads, functor_handle_));
}

void AwDrawFnImpl::PostDrawVk(AwDrawFn_PostDrawVkParams* params) {
  if (!vulkan_context_provider_)
    return;

  if (skip_next_post_draw_vk_) {
    skip_next_post_draw_vk_ = false;
    return;
  }

  DCHECK(scoped_secondary_cb_draw_);
  scoped_secondary_cb_draw_.reset();
}

void AwDrawFnImpl::RemoveOverlays(AwDrawFn_RemoveOverlaysParams* params) {
  DCHECK(params->merge_transaction);
  render_thread_manager_.RemoveOverlaysOnRT(params->merge_transaction);
}

}  // namespace android_webview

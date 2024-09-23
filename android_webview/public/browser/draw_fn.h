// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_FN_H_
#define ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_FN_H_

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;

// In order to make small changes backwards compatible, all structs passed from
// android to chromium are versioned.
//
// 1 is Android Q. This matches kAwDrawGLInfoVersion version 3.
// 2 Adds transfer_function_* and color_space_toXYZD50 to AwDrawFn_DrawGLParams.
// 3 Adds SurfaceControl related functions.
// 4 Adds AwDrawFn_ReportRenderingThreads to AwDrawFnFunctionTable.
static const int kAwDrawFnVersion = 4;

// Returns parent ASurfaceControl for WebView overlays. It will be have same
// geometry as the surface we draw into and positioned below it (underlay).
// This does not pass ownership to webview, but guaranteed to be alive until
// transaction from next removeOverlays call or functor destruction will be
// finished.
typedef ASurfaceControl* AwDrawFn_GetSurfaceControl();

// Merges WebView transaction to be applied synchronously with current draw.
// This doesn't pass ownership of the transaction, changes will be copied and
// webview can free transaction right after the call.
typedef void AwDrawFn_MergeTransaction(ASurfaceTransaction* transaction);

enum AwDrawFnOverlaysMode {
  // Indicated that webview should not promote anything to overlays this draw
  // and remove all visible overlays.
  // Added in version 3.
  AW_DRAW_FN_OVERLAYS_MODE_DISABLED = 0,

  // Indicates that webview can use overlays.
  // Added in version 3.
  AW_DRAW_FN_OVERLAYS_MODE_ENABLED = 1,
};

struct AwDrawFn_OnSyncParams {
  int version;

  bool apply_force_dark;
};

struct AwDrawFn_DrawGLParams {
  int version;

  // Input: current clip rect in surface coordinates. Reflects the current state
  // of the OpenGL scissor rect. Both the OpenGL scissor rect and viewport are
  // set by the caller of the draw function and updated during View animations.
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;

  // Input: current width/height of destination surface.
  int width;
  int height;

  // Used to be is_layer.
  bool deprecated_0;

  // Input: current transformation matrix in surface pixels.
  // Uses the column-based OpenGL matrix format.
  float transform[16];

  // Input: Color space parameters.
  float transfer_function_g;
  float transfer_function_a;
  float transfer_function_b;
  float transfer_function_c;
  float transfer_function_d;
  float transfer_function_e;
  float transfer_function_f;
  float color_space_toXYZD50[9];

  // Input: Indicates how webview should use overlays for this draw.
  // Added in version 3.
  AwDrawFnOverlaysMode overlays_mode;

  // Input: WebView can call it to obtain parent surface control for overlays.
  // Added in version 3.
  AwDrawFn_GetSurfaceControl* get_surface_control;

  // Input: WebView call this to apply ASurfaceTransaction synchronously with
  // the draw.
  // Added in version 3.
  AwDrawFn_MergeTransaction* merge_transaction;
};

struct AwDrawFn_InitVkParams {
  int version;
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  uint32_t graphics_queue_index;
  uint32_t api_version;
  const char* const* enabled_instance_extension_names;
  uint32_t enabled_instance_extension_names_length;
  const char* const* enabled_device_extension_names;
  uint32_t enabled_device_extension_names_length;
  // Only one of device_features and device_features_2 should be non-null.
  // If both are null then no features are enabled.
  VkPhysicalDeviceFeatures* device_features;
  VkPhysicalDeviceFeatures2* device_features_2;
};

struct AwDrawFn_DrawVkParams {
  int version;

  // Input: current width/height of destination surface.
  int width;
  int height;

  bool deprecated_0;

  // Input: current transform matrix
  float transform[16];

  // Input WebView should do its main compositing draws into this. It cannot do
  // anything that would require stopping the render pass.
  VkCommandBuffer secondary_command_buffer;

  // Input: The main color attachment index where secondary_command_buffer will
  // eventually be submitted.
  uint32_t color_attachment_index;

  // Input: A render pass which will be compatible to the one which the
  // secondary_command_buffer will be submitted into.
  VkRenderPass compatible_render_pass;

  // Input: Format of the destination surface.
  VkFormat format;

  // Input: Color space parameters.
  float transfer_function_g;
  float transfer_function_a;
  float transfer_function_b;
  float transfer_function_c;
  float transfer_function_d;
  float transfer_function_e;
  float transfer_function_f;
  float color_space_toXYZD50[9];

  // Input: current clip rect
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;

  // Input: Indicates how webview should use overlays for this draw.
  // Added in version 3.
  AwDrawFnOverlaysMode overlays_mode;

  // Input: WebView can call it to obtain parent surface control for overlays.
  // Added in version 3.
  AwDrawFn_GetSurfaceControl* get_surface_control;

  // Input: WebView call this to apply ASurfaceTransaction synchronously with
  // the draw.
  // Added in version 3.
  AwDrawFn_MergeTransaction* merge_transaction;
};

struct AwDrawFn_PostDrawVkParams {
  int version;
};

struct AwDrawFn_RemoveOverlaysParams {
  int version;
  // Input: WebView call this to apply ASurfaceTransaction synchronously with
  // the draw.
  // Added in version 3.
  AwDrawFn_MergeTransaction* merge_transaction;
};

// Called on render thread while UI thread is blocked. Called for both GL and
// VK.
typedef void AwDrawFn_OnSync(int functor,
                             void* data,
                             AwDrawFn_OnSyncParams* params);

// Called on render thread when either the context is destroyed _or_ when the
// functor's last reference goes away. Will always be called with an active
// context. Called for both GL and VK.
typedef void AwDrawFn_OnContextDestroyed(int functor, void* data);

// Called on render thread when the last reference to the handle goes away and
// the handle is considered irrevocably destroyed. Will always be preceded by
// a call to OnContextDestroyed if this functor had ever been drawn. Called for
// both GL and VK.
typedef void AwDrawFn_OnDestroyed(int functor, void* data);

// Only called for GL.
typedef void AwDrawFn_DrawGL(int functor,
                             void* data,
                             AwDrawFn_DrawGLParams* params);

// Initialize vulkan state. Needs to be called again after any
// OnContextDestroyed. Only called for Vulkan.
typedef void AwDrawFn_InitVk(int functor,
                             void* data,
                             AwDrawFn_InitVkParams* params);

// Only called for Vulkan.
typedef void AwDrawFn_DrawVk(int functor,
                             void* data,
                             AwDrawFn_DrawVkParams* params);

// Only called for Vulkan.
typedef void AwDrawFn_PostDrawVk(int functor,
                                 void* data,
                                 AwDrawFn_PostDrawVkParams* params);

// Can be called to make webview hide all overlays and stop updating them until
// next draw. WebView must obtain new ASurfaceControl after this call to use as
// parent for the overlays on next draw.
typedef void AwDrawFn_RemoveOverlays(int functor,
                                     void* data,
                                     AwDrawFn_RemoveOverlaysParams* params);

struct AwDrawFnFunctorCallbacks {
  // version is passed in CreateFunctor call.
  AwDrawFn_OnSync* on_sync;
  AwDrawFn_OnContextDestroyed* on_context_destroyed;
  AwDrawFn_OnDestroyed* on_destroyed;
  AwDrawFn_DrawGL* draw_gl;
  AwDrawFn_InitVk* init_vk;
  AwDrawFn_DrawVk* draw_vk;
  AwDrawFn_PostDrawVk* post_draw_vk;
  // Added in version 3.
  AwDrawFn_RemoveOverlays* remove_overlays;
};

enum AwDrawFnRenderMode {
  AW_DRAW_FN_RENDER_MODE_OPENGL_ES = 0,
  AW_DRAW_FN_RENDER_MODE_VULKAN = 1,
};

// Get the render mode. Result is static for the process.
typedef AwDrawFnRenderMode AwDrawFn_QueryRenderMode(void);

// This available up to version 3, use the one below.
typedef int AwDrawFn_CreateFunctor(void* data,
                                   AwDrawFnFunctorCallbacks* functor_callbacks);

// Create a functor. |functor_callbacks| should be valid until OnDestroyed.
typedef int AwDrawFn_CreateFunctor_v3(
    void* data,
    int version,
    AwDrawFnFunctorCallbacks* functor_callbacks);

// May be called on any thread to signal that the functor should be destroyed.
// The functor will receive an onDestroyed when the last usage of it is
// released, and it should be considered alive & active until that point.
typedef void AwDrawFn_ReleaseFunctor(int functor);

// Report the list of threads critical for frame production for the given
// functor. Must be called on render thread.
typedef void AwDrawFn_ReportRenderingThreads(int functor,
                                             const int32_t* thread_ids,
                                             size_t size);

struct AwDrawFnFunctionTable {
  int version;
  AwDrawFn_QueryRenderMode* query_render_mode;
  // Available up to version 3.
  AwDrawFn_CreateFunctor* create_functor;
  AwDrawFn_ReleaseFunctor* release_functor;
  // Added in version 3.
  AwDrawFn_CreateFunctor_v3* create_functor_v3;
  // Added in version 4.
  AwDrawFn_ReportRenderingThreads* report_rendering_threads;
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_FN_H_

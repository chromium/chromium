// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/gvr-android-keyboard/src/libraries/headers/vr/gvr/capi/include/gvr_keyboard.h"

#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>
#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "chrome/android/features/vr/jni_headers/GvrKeyboardLoaderClient_jni.h"

namespace {

// Run CALL macro for every function defined in the API.
#define FOR_EACH_API_FN                                             \
  CALL(gvr_keyboard_initialize)                                     \
  CALL(gvr_keyboard_create)                                         \
  CALL(gvr_keyboard_get_input_mode)                                 \
  CALL(gvr_keyboard_set_input_mode)                                 \
  CALL(gvr_keyboard_get_recommended_world_from_keyboard_matrix)     \
  CALL(gvr_keyboard_set_world_from_keyboard_matrix)                 \
  CALL(gvr_keyboard_show)                                           \
  CALL(gvr_keyboard_update_button_state)                            \
  CALL(gvr_keyboard_update_controller_ray)                          \
  CALL(gvr_keyboard_update_controller_touch)                        \
  CALL(gvr_keyboard_get_text)                                       \
  CALL(gvr_keyboard_set_text)                                       \
  CALL(gvr_keyboard_get_selection_indices)                          \
  CALL(gvr_keyboard_set_selection_indices)                          \
  CALL(gvr_keyboard_get_composing_indices)                          \
  CALL(gvr_keyboard_set_composing_indices)                          \
  CALL(gvr_keyboard_set_frame_time)                                 \
  CALL(gvr_keyboard_set_eye_from_world_matrix)                      \
  CALL(gvr_keyboard_set_projection_matrix)                          \
  CALL(gvr_keyboard_set_viewport)                                   \
  CALL(gvr_keyboard_advance_frame)                                  \
  CALL(gvr_keyboard_render)                                         \
  CALL(gvr_keyboard_hide)                                           \
  CALL(gvr_keyboard_destroy)                                        \
  OPTIONAL_CALL(gvr_keyboard_set_anti_alias_enabled)                \
  OPTIONAL_CALL(gvr_keyboard_set_voice_input_enabled)               \
  OPTIONAL_CALL(gvr_keyboard_set_voice_permission_callback_enabled) \
  OPTIONAL_CALL(gvr_keyboard_request_voice_permission)              \
  OPTIONAL_CALL(gvr_keyboard_set_multiview_enabled)                 \
  OPTIONAL_CALL(gvr_keyboard_multiview_set_viewport)                \
  OPTIONAL_CALL(gvr_keyboard_multiview_render)                      \
  OPTIONAL_CALL(gvr_keyboard_get_hit_normal)

// The min API version that is guaranteed to exists on the user's device if they
// have some version of the Daydream keyboard installed.
constexpr int64_t kMinExpectedApiVersion = 1;

// The version of the Daydream keyboard that supports text selection.
constexpr int64_t kSelectionSupportApiVersion = 2;

// Infer text selection support by checking for the presence of an unrelated
// method, added in the same release as selection.
constexpr char kSelectionSymbol[] = "gvr_keyboard_update_controller_touch";

#define CALL(fn) decltype(&fn) impl_##fn = nullptr;
#define OPTIONAL_CALL(fn) decltype(&fn) impl_##fn = nullptr;
struct KeyboardApi {
  explicit KeyboardApi(int64_t version) : min_version(version) {}

  // The loaded API version is at least this version.
  int64_t min_version;

  FOR_EACH_API_FN
};
#undef CALL
#undef OPTIONAL_CALL

static void* sdk_handle = nullptr;

static KeyboardApi* keyboard_api = nullptr;

template <typename Fn>
bool LoadFunction(void* handle, const char* function_name, Fn* fn_out) {
  void* fn = dlsym(handle, function_name);
  if (!fn)
    return false;

  *fn_out = reinterpret_cast<Fn>(fn);
  return true;
}

void CloseSdk() {
  if (!sdk_handle)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  vr::Java_GvrKeyboardLoaderClient_closeKeyboardSDK(
      env, reinterpret_cast<jlong>(sdk_handle));

// Null all the function pointers.
#define CALL(fn) keyboard_api->impl_##fn = nullptr;
#define OPTIONAL_CALL(fn) keyboard_api->impl_##fn = nullptr;
  FOR_EACH_API_FN
#undef CALL
#undef OPTIONAL_CALL

  sdk_handle = nullptr;
  keyboard_api = nullptr;
}

bool LoadSdk(void* closure, gvr_keyboard_callback callback) {
  if (sdk_handle)
    return true;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  base::android::ScopedJavaLocalRef<jobject> context_wrapper =
      vr::Java_GvrKeyboardLoaderClient_getContextWrapper(env);

  base::android::ScopedJavaLocalRef<jobject> remote_class_loader =
      vr::Java_GvrKeyboardLoaderClient_getRemoteClassLoader(env);

  sdk_handle = reinterpret_cast<void*>(
      vr::Java_GvrKeyboardLoaderClient_loadKeyboardSDK(env));

  if (!sdk_handle) {
    LOG(ERROR) << "Failed to load GVR keyboard SDK.";
    return false;
  }

  // We currently don't have an API that gives us the actual version of the
  // keyboard, so we extract the minimum version based on the API methods that
  // we can dynamically load.
  int64_t min_api_version = kMinExpectedApiVersion;
  if (dlsym(sdk_handle, kSelectionSymbol) != nullptr) {
    min_api_version = kSelectionSupportApiVersion;
  }

  // Load all function pointers from the SDK.
  DCHECK(!keyboard_api);
  keyboard_api = new KeyboardApi(min_api_version);

  bool success = true;
// Load all function pointers from SDK.
#define CALL(fn) \
  success &= LoadFunction(sdk_handle, #fn, &keyboard_api->impl_##fn);
#define OPTIONAL_CALL(fn) \
  LoadFunction(sdk_handle, #fn, &keyboard_api->impl_##fn);
  FOR_EACH_API_FN
#undef CALL
#undef OPTIONAL_CALL

  DCHECK(success);

  gvr_keyboard_initialize(env, context_wrapper.obj(),
                          remote_class_loader.obj());
  return true;
}

#undef FOR_EACH_API_FN
}  // namespace

void gvr_keyboard_initialize(JNIEnv* env,
                             jobject app_context,
                             jobject class_loader) {
  keyboard_api->impl_gvr_keyboard_initialize(env, app_context, class_loader);
}

gvr_keyboard_context* gvr_keyboard_create(void* closure,
                                          gvr_keyboard_callback callback) {
  if (!LoadSdk(closure, callback)) {
    return nullptr;
  }
  return keyboard_api->impl_gvr_keyboard_create(closure, callback);
}

bool gvr_keyboard_set_anti_alias_enabled(bool enabled) {
  if (keyboard_api->impl_gvr_keyboard_set_anti_alias_enabled) {
    keyboard_api->impl_gvr_keyboard_set_anti_alias_enabled(enabled);
    return true;
  }
  return false;
}

bool gvr_keyboard_set_voice_input_enabled(bool enabled) {
  if (keyboard_api->impl_gvr_keyboard_set_voice_input_enabled) {
    keyboard_api->impl_gvr_keyboard_set_voice_input_enabled(enabled);
    return true;
  }
  return false;
}

bool gvr_keyboard_set_voice_permission_callback_enabled(bool enabled) {
  if (keyboard_api->impl_gvr_keyboard_set_voice_permission_callback_enabled) {
    keyboard_api->impl_gvr_keyboard_set_voice_permission_callback_enabled(
        enabled);
    return true;
  }
  return false;
}

bool gvr_keyboard_request_voice_permission(gvr_keyboard_context* context) {
  if (keyboard_api->impl_gvr_keyboard_request_voice_permission) {
    return keyboard_api->impl_gvr_keyboard_request_voice_permission(context);
  }
  return false;
}

bool gvr_keyboard_set_multiview_enabled(bool enabled) {
  if (keyboard_api->impl_gvr_keyboard_set_multiview_enabled) {
    keyboard_api->impl_gvr_keyboard_set_multiview_enabled(enabled);
    return true;
  }
  return false;
}

void gvr_keyboard_destroy(gvr_keyboard_context** context) {
  keyboard_api->impl_gvr_keyboard_destroy(context);
  CloseSdk();
}

int32_t gvr_keyboard_get_input_mode(gvr_keyboard_context* context) {
  return keyboard_api->impl_gvr_keyboard_get_input_mode(context);
}

void gvr_keyboard_set_input_mode(gvr_keyboard_context* context,
                                 int32_t input_mode) {
  keyboard_api->impl_gvr_keyboard_set_input_mode(context, input_mode);
}

void gvr_keyboard_get_recommended_world_from_keyboard_matrix(
    float distance_from_eye,
    gvr_mat4f* matrix) {
  keyboard_api->impl_gvr_keyboard_get_recommended_world_from_keyboard_matrix(
      distance_from_eye, matrix);
}

void gvr_keyboard_set_world_from_keyboard_matrix(gvr_keyboard_context* context,
                                                 const gvr_mat4f* matrix) {
  keyboard_api->impl_gvr_keyboard_set_world_from_keyboard_matrix(context,
                                                                 matrix);
}

void gvr_keyboard_show(gvr_keyboard_context* context) {
  keyboard_api->impl_gvr_keyboard_show(context);
}

void gvr_keyboard_update_button_state(gvr_keyboard_context* context,
                                      int32_t button_index,
                                      bool pressed) {
  keyboard_api->impl_gvr_keyboard_update_button_state(context, button_index,
                                                      pressed);
}

bool gvr_keyboard_update_controller_ray(gvr_keyboard_context* context,
                                        const gvr_vec3f* start,
                                        const gvr_vec3f* end,
                                        gvr_vec3f* hit) {
  return keyboard_api->impl_gvr_keyboard_update_controller_ray(context, start,
                                                               end, hit);
}

bool gvr_keyboard_get_hit_normal(gvr_keyboard_context* context,
                                 gvr_vec3f* normal) {
  if (keyboard_api->impl_gvr_keyboard_get_hit_normal) {
    return keyboard_api->impl_gvr_keyboard_get_hit_normal(context, normal);
  }
  return false;
}

void gvr_keyboard_update_controller_touch(gvr_keyboard_context* context,
                                          bool touched,
                                          const gvr_vec2f* pos) {
  if (!keyboard_api->impl_gvr_keyboard_update_controller_touch)
    return;

  return keyboard_api->impl_gvr_keyboard_update_controller_touch(context,
                                                                 touched, pos);
}

char* gvr_keyboard_get_text(gvr_keyboard_context* context) {
  return keyboard_api->impl_gvr_keyboard_get_text(context);
}

void gvr_keyboard_set_text(gvr_keyboard_context* context, const char* text) {
  return keyboard_api->impl_gvr_keyboard_set_text(context, text);
}

void gvr_keyboard_get_selection_indices(gvr_keyboard_context* context,
                                        size_t* start,
                                        size_t* end) {
  keyboard_api->impl_gvr_keyboard_get_selection_indices(context, start, end);
}

void gvr_keyboard_set_selection_indices(gvr_keyboard_context* context,
                                        size_t start,
                                        size_t end) {
  keyboard_api->impl_gvr_keyboard_set_selection_indices(context, start, end);
}

void gvr_keyboard_get_composing_indices(gvr_keyboard_context* context,
                                        size_t* start,
                                        size_t* end) {
  keyboard_api->impl_gvr_keyboard_get_composing_indices(context, start, end);
}

void gvr_keyboard_set_composing_indices(gvr_keyboard_context* context,
                                        size_t start,
                                        size_t end) {
  keyboard_api->impl_gvr_keyboard_set_composing_indices(context, start, end);
}

void gvr_keyboard_set_frame_time(gvr_keyboard_context* context,
                                 const gvr_clock_time_point* time) {
  keyboard_api->impl_gvr_keyboard_set_frame_time(context, time);
}

void gvr_keyboard_set_eye_from_world_matrix(gvr_keyboard_context* context,
                                            int32_t eye_type,
                                            const gvr_mat4f* matrix) {
  keyboard_api->impl_gvr_keyboard_set_eye_from_world_matrix(context, eye_type,
                                                            matrix);
}

void gvr_keyboard_set_projection_matrix(gvr_keyboard_context* context,
                                        int32_t eye_type,
                                        const gvr_mat4f* projection) {
  keyboard_api->impl_gvr_keyboard_set_projection_matrix(context, eye_type,
                                                        projection);
}

void gvr_keyboard_multiview_set_viewport(gvr_keyboard_context* context,
                                         const gvr_recti* viewport) {
  if (keyboard_api->impl_gvr_keyboard_multiview_set_viewport) {
    keyboard_api->impl_gvr_keyboard_multiview_set_viewport(context, viewport);
  }
}

void gvr_keyboard_set_viewport(gvr_keyboard_context* context,
                               int32_t eye_type,
                               const gvr_recti* viewport) {
  keyboard_api->impl_gvr_keyboard_set_viewport(context, eye_type, viewport);
}

void gvr_keyboard_advance_frame(gvr_keyboard_context* context) {
  keyboard_api->impl_gvr_keyboard_advance_frame(context);
}

void gvr_keyboard_render(gvr_keyboard_context* context, int32_t eye_type) {
  keyboard_api->impl_gvr_keyboard_render(context, eye_type);
}

void gvr_keyboard_multiview_render(gvr_keyboard_context* context) {
  if (keyboard_api->impl_gvr_keyboard_multiview_render) {
    keyboard_api->impl_gvr_keyboard_multiview_render(context);
  }
}
void gvr_keyboard_hide(gvr_keyboard_context* context) {
  keyboard_api->impl_gvr_keyboard_hide(context);
}

bool gvr_keyboard_supports_selection() {
  if (!keyboard_api)
    return false;
  return keyboard_api->min_version >= kSelectionSupportApiVersion;
}

int64_t gvr_keyboard_version() {
  if (!keyboard_api)
    return -1;
  return keyboard_api->min_version;
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/arcore-android-sdk/src/libraries/include/arcore_c_api.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Run CALL macro for every function defined in the API.
#define FOR_EACH_API_FN                  \
  CALL(ArCamera_getDisplayOrientedPose)  \
  CALL(ArCamera_getProjectionMatrix)     \
  CALL(ArCamera_getTrackingState)        \
  CALL(ArCamera_getViewMatrix)           \
  CALL(ArConfig_create)                  \
  CALL(ArConfig_destroy)                 \
  CALL(ArFrame_acquireCamera)            \
  CALL(ArFrame_create)                   \
  CALL(ArFrame_destroy)                  \
  CALL(ArFrame_hitTest)                  \
  CALL(ArFrame_transformDisplayUvCoords) \
  CALL(ArHitResult_create)               \
  CALL(ArHitResult_destroy)              \
  CALL(ArHitResult_getHitPose)           \
  CALL(ArHitResultList_create)           \
  CALL(ArHitResultList_destroy)          \
  CALL(ArHitResultList_getItem)          \
  CALL(ArHitResultList_getSize)          \
  CALL(ArPose_create)                    \
  CALL(ArPose_destroy)                   \
  CALL(ArPose_getMatrix)                 \
  CALL(ArPose_getPoseRaw)                \
  CALL(ArSession_checkSupported)         \
  CALL(ArSession_configure)              \
  CALL(ArSession_create)                 \
  CALL(ArSession_destroy)                \
  CALL(ArSession_pause)                  \
  CALL(ArSession_resume)                 \
  CALL(ArSession_setCameraTextureName)   \
  CALL(ArSession_setDisplayGeometry)     \
  CALL(ArHitResult_acquireTrackable)     \
  CALL(ArTrackable_getType)              \
  CALL(ArTrackable_release)              \
  CALL(ArPlane_isPoseInPolygon)          \
  CALL(ArSession_update)

#define CALL(fn) decltype(&fn) impl_##fn = nullptr;
struct ArCoreApi {
  FOR_EACH_API_FN
};
#undef CALL

static void* sdk_handle = nullptr;
static ArCoreApi* arcore_api = nullptr;

template <typename Fn>
void LoadFunction(void* handle, const char* function_name, Fn* fn_out) {
  void* fn = dlsym(handle, function_name);
  if (!fn)
    return;

  *fn_out = reinterpret_cast<Fn>(fn);
}

}  // namespace

namespace vr {

bool LoadArCoreSdk(const std::string& libraryPath) {
  if (arcore_api)
    return true;

  sdk_handle = dlopen(libraryPath.c_str(), RTLD_GLOBAL | RTLD_NOW);
  if (!sdk_handle) {
    char* error_string = nullptr;
    error_string = dlerror();
    LOG(ERROR) << "Could not open libarcore_sdk_c_minimal.so: " << error_string;
    return false;
  } else {
    VLOG(2) << "Opened shim shared library.";
  }

  // TODO(vollick): check SDK version.
  arcore_api = new ArCoreApi();

#define CALL(fn) LoadFunction(sdk_handle, #fn, &arcore_api->impl_##fn);
  FOR_EACH_API_FN
#undef CALL

  return true;
}

}  // namespace vr

#undef FOR_EACH_API_FN

void ArCamera_getDisplayOrientedPose(const ArSession* session,
                                     const ArCamera* camera,
                                     ArPose* out_pose) {
  arcore_api->impl_ArCamera_getDisplayOrientedPose(session, camera, out_pose);
}

void ArCamera_getProjectionMatrix(const ArSession* session,
                                  const ArCamera* camera,
                                  float near,
                                  float far,
                                  float* dest_col_major_4x4) {
  arcore_api->impl_ArCamera_getProjectionMatrix(session, camera, near, far,
                                                dest_col_major_4x4);
}
void ArCamera_getTrackingState(const ArSession* session,
                               const ArCamera* camera,
                               ArTrackingState* out_tracking_state) {
  arcore_api->impl_ArCamera_getTrackingState(session, camera,
                                             out_tracking_state);
}

void ArCamera_getViewMatrix(const ArSession* session,
                            const ArCamera* camera,
                            float* out_matrix) {
  arcore_api->impl_ArCamera_getViewMatrix(session, camera, out_matrix);
}

void ArConfig_create(const ArSession* session, ArConfig** out_config) {
  arcore_api->impl_ArConfig_create(session, out_config);
}

void ArConfig_destroy(ArConfig* config) {
  arcore_api->impl_ArConfig_destroy(config);
}

void ArFrame_acquireCamera(const ArSession* session,
                           const ArFrame* frame,
                           ArCamera** out_camera) {
  arcore_api->impl_ArFrame_acquireCamera(session, frame, out_camera);
}

void ArFrame_create(const ArSession* session, ArFrame** out_frame) {
  arcore_api->impl_ArFrame_create(session, out_frame);
}

void ArFrame_destroy(ArFrame* frame) {
  arcore_api->impl_ArFrame_destroy(frame);
}

void ArFrame_hitTest(const ArSession* session,
                     const ArFrame* frame,
                     float pixel_x,
                     float pixel_y,
                     ArHitResultList* out_hit_results) {
  arcore_api->impl_ArFrame_hitTest(session, frame, pixel_x, pixel_y,
                                   out_hit_results);
}

void ArFrame_transformDisplayUvCoords(const ArSession* session,
                                      const ArFrame* frame,
                                      int32_t num_elements,
                                      const float* uvs_in,
                                      float* uvs_out) {
  arcore_api->impl_ArFrame_transformDisplayUvCoords(
      session, frame, num_elements, uvs_in, uvs_out);
}

void ArHitResult_create(const ArSession* session,
                        ArHitResult** out_hit_result) {
  arcore_api->impl_ArHitResult_create(session, out_hit_result);
}

void ArHitResult_destroy(ArHitResult* hit_result) {
  arcore_api->impl_ArHitResult_destroy(hit_result);
}

void ArHitResult_getHitPose(const ArSession* session,
                            const ArHitResult* hit_result,
                            ArPose* out_pose) {
  arcore_api->impl_ArHitResult_getHitPose(session, hit_result, out_pose);
}

void ArHitResult_acquireTrackable(const ArSession* session,
                                  const ArHitResult* hit_result,
                                  ArTrackable** out_trackable) {
  arcore_api->impl_ArHitResult_acquireTrackable(session, hit_result,
                                                out_trackable);
}

void ArTrackable_getType(const ArSession* session,
                         const ArTrackable* trackable,
                         ArTrackableType* out_trackable_type) {
  arcore_api->impl_ArTrackable_getType(session, trackable, out_trackable_type);
}

void ArPlane_isPoseInPolygon(const ArSession* session,
                             const ArPlane* plane,
                             const ArPose* pose,
                             int32_t* out_pose_in_polygon) {
  arcore_api->impl_ArPlane_isPoseInPolygon(session, plane, pose,
                                           out_pose_in_polygon);
}

void ArTrackable_release(ArTrackable* trackable) {
  arcore_api->impl_ArTrackable_release(trackable);
}

void ArHitResultList_create(const ArSession* session,
                            ArHitResultList** out_hit_result_list) {
  arcore_api->impl_ArHitResultList_create(session, out_hit_result_list);
}

void ArHitResultList_destroy(ArHitResultList* hit_result_list) {
  arcore_api->impl_ArHitResultList_destroy(hit_result_list);
}

void ArHitResultList_getItem(const ArSession* session,
                             const ArHitResultList* hit_result_list,
                             int index,
                             ArHitResult* out_hit_result) {
  arcore_api->impl_ArHitResultList_getItem(session, hit_result_list, index,
                                           out_hit_result);
}

void ArHitResultList_getSize(const ArSession* session,
                             const ArHitResultList* hit_result_list,
                             int* out_size) {
  arcore_api->impl_ArHitResultList_getSize(session, hit_result_list, out_size);
}

void ArPose_create(const ArSession* session,
                   const float* pose_raw,
                   ArPose** out_pose) {
  arcore_api->impl_ArPose_create(session, pose_raw, out_pose);
}

void ArPose_destroy(ArPose* pose) {
  arcore_api->impl_ArPose_destroy(pose);
}

void ArPose_getMatrix(const ArSession* session,
                      const ArPose* pose,
                      float* out_matrix) {
  arcore_api->impl_ArPose_getMatrix(session, pose, out_matrix);
}

void ArPose_getPoseRaw(const ArSession* session,
                       const ArPose* pose,
                       float* out_pose_raw) {
  arcore_api->impl_ArPose_getPoseRaw(session, pose, out_pose_raw);
}

ArStatus ArSession_checkSupported(const ArSession* session,
                                  const ArConfig* config) {
  return arcore_api->impl_ArSession_checkSupported(session, config);
}

ArStatus ArSession_configure(ArSession* session, const ArConfig* config) {
  return arcore_api->impl_ArSession_configure(session, config);
}

ArStatus ArSession_create(void* env,
                          void* application_context,
                          ArSession** out_session_pointer) {
  return arcore_api->impl_ArSession_create(env, application_context,
                                           out_session_pointer);
}

void ArSession_destroy(ArSession* session) {
  arcore_api->impl_ArSession_destroy(session);
}

ArStatus ArSession_pause(ArSession* session) {
  return arcore_api->impl_ArSession_pause(session);
}

ArStatus ArSession_resume(ArSession* session) {
  return arcore_api->impl_ArSession_resume(session);
}

void ArSession_setCameraTextureName(ArSession* session, uint32_t texture_id) {
  return arcore_api->impl_ArSession_setCameraTextureName(session, texture_id);
}

void ArSession_setDisplayGeometry(ArSession* session,
                                  int32_t rotation,
                                  int32_t width,
                                  int32_t height) {
  return arcore_api->impl_ArSession_setDisplayGeometry(session, rotation, width,
                                                       height);
}

ArStatus ArSession_update(ArSession* session, ArFrame* out_frame) {
  return arcore_api->impl_ArSession_update(session, out_frame);
}

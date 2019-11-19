// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_sdk.h"

#include <dlfcn.h>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/logging.h"

namespace {

// Run CALL macro for every function defined in the API.
#define FOR_EACH_API_FN                       \
  CALL(ArAnchorList_acquireItem)              \
  CALL(ArAnchorList_create)                   \
  CALL(ArAnchorList_destroy)                  \
  CALL(ArAnchorList_getSize)                  \
  CALL(ArAnchor_detach)                       \
  CALL(ArAnchor_getPose)                      \
  CALL(ArAnchor_getTrackingState)             \
  CALL(ArAnchor_release)                      \
  CALL(ArCamera_getDisplayOrientedPose)       \
  CALL(ArCamera_getProjectionMatrix)          \
  CALL(ArCamera_getTrackingState)             \
  CALL(ArCamera_getViewMatrix)                \
  CALL(ArConfig_create)                       \
  CALL(ArConfig_destroy)                      \
  CALL(ArFrame_acquireCamera)                 \
  CALL(ArFrame_create)                        \
  CALL(ArFrame_destroy)                       \
  CALL(ArFrame_getUpdatedAnchors)             \
  CALL(ArFrame_getUpdatedTrackables)          \
  CALL(ArFrame_hitTestRay)                    \
  CALL(ArFrame_transformCoordinates2d)        \
  CALL(ArHitResult_acquireTrackable)          \
  CALL(ArHitResult_create)                    \
  CALL(ArHitResult_destroy)                   \
  CALL(ArHitResult_getHitPose)                \
  CALL(ArHitResultList_create)                \
  CALL(ArHitResultList_destroy)               \
  CALL(ArHitResultList_getItem)               \
  CALL(ArHitResultList_getSize)               \
  CALL(ArPlane_acquireSubsumedBy)             \
  CALL(ArPlane_getCenterPose)                 \
  CALL(ArPlane_getPolygon)                    \
  CALL(ArPlane_getPolygonSize)                \
  CALL(ArPlane_getType)                       \
  CALL(ArPlane_isPoseInPolygon)               \
  CALL(ArPose_create)                         \
  CALL(ArPose_destroy)                        \
  CALL(ArPose_getMatrix)                      \
  CALL(ArPose_getPoseRaw)                     \
  CALL(ArSession_acquireNewAnchor)            \
  CALL(ArSession_configure)                   \
  CALL(ArSession_create)                      \
  CALL(ArSession_destroy)                     \
  CALL(ArSession_enableIncognitoMode_private) \
  CALL(ArSession_getAllAnchors)               \
  CALL(ArSession_getAllTrackables)            \
  CALL(ArSession_pause)                       \
  CALL(ArSession_resume)                      \
  CALL(ArSession_setCameraTextureName)        \
  CALL(ArSession_setDisplayGeometry)          \
  CALL(ArSession_update)                      \
  CALL(ArTrackable_acquireNewAnchor)          \
  CALL(ArTrackable_getTrackingState)          \
  CALL(ArTrackable_getType)                   \
  CALL(ArTrackable_release)                   \
  CALL(ArTrackableList_acquireItem)           \
  CALL(ArTrackableList_create)                \
  CALL(ArTrackableList_destroy)               \
  CALL(ArTrackableList_getSize)

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
    LOG(ERROR) << "Could not open libarcore_sdk_c.so: " << error_string;
    return false;
  } else {
    VLOG(2) << "Opened shim shared library.";
  }

  // TODO(https://crbug.com/914999): check SDK version.
  arcore_api = new ArCoreApi();

#define CALL(fn) LoadFunction(sdk_handle, #fn, &arcore_api->impl_##fn);
  FOR_EACH_API_FN
#undef CALL

  return true;
}

bool IsArCoreSupported() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_NOUGAT;
}

}  // namespace vr

#undef FOR_EACH_API_FN

void ArAnchorList_acquireItem(const ArSession* session,
                              const ArAnchorList* anchor_list,
                              int32_t index,
                              ArAnchor** out_anchor) {
  arcore_api->impl_ArAnchorList_acquireItem(session, anchor_list, index,
                                            out_anchor);
}

void ArAnchorList_create(const ArSession* session,
                         ArAnchorList** out_anchor_list) {
  arcore_api->impl_ArAnchorList_create(session, out_anchor_list);
}

void ArAnchorList_destroy(ArAnchorList* anchor_list) {
  arcore_api->impl_ArAnchorList_destroy(anchor_list);
}

void ArAnchorList_getSize(const ArSession* session,
                          const ArAnchorList* anchor_list,
                          int32_t* out_size) {
  arcore_api->impl_ArAnchorList_getSize(session, anchor_list, out_size);
}

void ArAnchor_detach(ArSession* session, ArAnchor* anchor) {
  arcore_api->impl_ArAnchor_detach(session, anchor);
}

void ArAnchor_getPose(const ArSession* session,
                      const ArAnchor* anchor,
                      ArPose* out_pose) {
  arcore_api->impl_ArAnchor_getPose(session, anchor, out_pose);
}

void ArAnchor_getTrackingState(const ArSession* session,
                               const ArAnchor* anchor,
                               ArTrackingState* out_tracking_state) {
  arcore_api->impl_ArAnchor_getTrackingState(session, anchor,
                                             out_tracking_state);
}

void ArAnchor_release(ArAnchor* anchor) {
  arcore_api->impl_ArAnchor_release(anchor);
}

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

void ArFrame_getUpdatedAnchors(const ArSession* session,
                               const ArFrame* frame,
                               ArAnchorList* out_anchor_list) {
  arcore_api->impl_ArFrame_getUpdatedAnchors(session, frame, out_anchor_list);
}

void ArFrame_getUpdatedTrackables(const ArSession* session,
                                  const ArFrame* frame,
                                  ArTrackableType filter_type,
                                  ArTrackableList* out_trackable_list) {
  arcore_api->impl_ArFrame_getUpdatedTrackables(session, frame, filter_type,
                                                out_trackable_list);
}

void ArFrame_hitTestRay(const ArSession* session,
                        const ArFrame* frame,
                        const float* ray_origin_3,
                        const float* ray_direction_3,
                        ArHitResultList* out_hit_results) {
  arcore_api->impl_ArFrame_hitTestRay(session, frame, ray_origin_3,
                                      ray_direction_3, out_hit_results);
}

void ArFrame_transformCoordinates2d(const ArSession* session,
                                    const ArFrame* frame,
                                    ArCoordinates2dType input_coordinates,
                                    int32_t number_of_vertices,
                                    const float* vertices_2d,
                                    ArCoordinates2dType output_coordinates,
                                    float* out_vertices_2d) {
  arcore_api->impl_ArFrame_transformCoordinates2d(
      session, frame, input_coordinates, number_of_vertices, vertices_2d,
      output_coordinates, out_vertices_2d);
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

ArStatus ArTrackable_acquireNewAnchor(ArSession* session,
                                      ArTrackable* trackable,
                                      ArPose* pose,
                                      ArAnchor** out_anchor) {
  return arcore_api->impl_ArTrackable_acquireNewAnchor(session, trackable, pose,
                                                       out_anchor);
}

void ArTrackable_getTrackingState(const ArSession* session,
                                  const ArTrackable* trackable,
                                  ArTrackingState* out_tracking_state) {
  arcore_api->impl_ArTrackable_getTrackingState(session, trackable,
                                                out_tracking_state);
}

void ArTrackable_getType(const ArSession* session,
                         const ArTrackable* trackable,
                         ArTrackableType* out_trackable_type) {
  arcore_api->impl_ArTrackable_getType(session, trackable, out_trackable_type);
}

void ArTrackable_release(ArTrackable* trackable) {
  arcore_api->impl_ArTrackable_release(trackable);
}

void ArTrackableList_acquireItem(const ArSession* session,
                                 const ArTrackableList* trackable_list,
                                 int32_t index,
                                 ArTrackable** out_trackable) {
  arcore_api->impl_ArTrackableList_acquireItem(session, trackable_list, index,
                                               out_trackable);
}

void ArTrackableList_create(const ArSession* session,
                            ArTrackableList** out_trackable_list) {
  arcore_api->impl_ArTrackableList_create(session, out_trackable_list);
}

void ArTrackableList_destroy(ArTrackableList* trackable_list) {
  arcore_api->impl_ArTrackableList_destroy(trackable_list);
}

void ArTrackableList_getSize(const ArSession* session,
                             const ArTrackableList* trackable_list,
                             int32_t* out_size) {
  arcore_api->impl_ArTrackableList_getSize(session, trackable_list, out_size);
}

void ArPlane_acquireSubsumedBy(const ArSession* session,
                               const ArPlane* plane,
                               ArPlane** out_subsumed_by) {
  arcore_api->impl_ArPlane_acquireSubsumedBy(session, plane, out_subsumed_by);
}

void ArPlane_getCenterPose(const ArSession* session,
                           const ArPlane* plane,
                           ArPose* out_pose) {
  arcore_api->impl_ArPlane_getCenterPose(session, plane, out_pose);
}

void ArPlane_getPolygon(const ArSession* session,
                        const ArPlane* plane,
                        float* out_polygon_xz) {
  arcore_api->impl_ArPlane_getPolygon(session, plane, out_polygon_xz);
}

void ArPlane_getPolygonSize(const ArSession* session,
                            const ArPlane* plane,
                            int32_t* out_polygon_size) {
  arcore_api->impl_ArPlane_getPolygonSize(session, plane, out_polygon_size);
}

void ArPlane_getType(const ArSession* session,
                     const ArPlane* plane,
                     ArPlaneType* out_plane_type) {
  arcore_api->impl_ArPlane_getType(session, plane, out_plane_type);
}

void ArPlane_isPoseInPolygon(const ArSession* session,
                             const ArPlane* plane,
                             const ArPose* pose,
                             int32_t* out_pose_in_polygon) {
  arcore_api->impl_ArPlane_isPoseInPolygon(session, plane, pose,
                                           out_pose_in_polygon);
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

ArStatus ArSession_acquireNewAnchor(ArSession* session,
                                    const ArPose* pose,
                                    ArAnchor** out_anchor) {
  return arcore_api->impl_ArSession_acquireNewAnchor(session, pose, out_anchor);
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

void ArSession_enableIncognitoMode_private(ArSession* session) {
  arcore_api->impl_ArSession_enableIncognitoMode_private(session);
}

void ArSession_getAllAnchors(const ArSession* session,
                             ArAnchorList* out_anchor_list) {
  arcore_api->impl_ArSession_getAllAnchors(session, out_anchor_list);
}

void ArSession_getAllTrackables(const ArSession* session,
                                ArTrackableType filter_type,
                                ArTrackableList* out_trackable_list) {
  arcore_api->impl_ArSession_getAllTrackables(session, filter_type,
                                              out_trackable_list);
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

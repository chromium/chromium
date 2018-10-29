// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_generic.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/arcore-android-sdk/src/libraries/include/arcore_c_api.h"

namespace device {

namespace internal {

template <class T>
struct ScopedGenericArObject {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) {}
};

template <>
void inline ScopedGenericArObject<ArSession*>::Free(ArSession* ar_session) {
  ArSession_destroy(ar_session);
}

template <>
void inline ScopedGenericArObject<ArFrame*>::Free(ArFrame* ar_frame) {
  ArFrame_destroy(ar_frame);
}

template <>
void inline ScopedGenericArObject<ArConfig*>::Free(ArConfig* ar_config) {
  ArConfig_destroy(ar_config);
}

template <>
void inline ScopedGenericArObject<ArPose*>::Free(ArPose* ar_pose) {
  ArPose_destroy(ar_pose);
}

template <>
void inline ScopedGenericArObject<ArCamera*>::Free(ArCamera* ar_camera) {
  // Do nothing - ArCamera has no destroy method and is managed by ArCore.
}

template <>
void inline ScopedGenericArObject<ArHitResultList*>::Free(
    ArHitResultList* ar_hit_result_list) {
  ArHitResultList_destroy(ar_hit_result_list);
}

template <>
void inline ScopedGenericArObject<ArHitResult*>::Free(
    ArHitResult* ar_hit_result) {
  ArHitResult_destroy(ar_hit_result);
}

template <class T>
using ScopedArCoreObject = base::ScopedGeneric<T, ScopedGenericArObject<T>>;

}  // namespace internal

// This class should be created and accessed entirely on a Gl thread.
class ArCoreImpl : public ArCore {
 public:
  ArCoreImpl();
  ~ArCoreImpl() override;

  bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  void SetCameraTexture(GLuint camera_texture_id) override;
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;
  void Pause() override;
  void Resume() override;
  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      const gfx::Size& image_size,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

 private:
  bool TransformRayToScreenSpace(const mojom::XRRayPtr& ray,
                                 const gfx::Size& image_size,
                                 gfx::PointF* screen_point);

  bool IsOnGlThread();
  base::WeakPtr<ArCoreImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // An ArCore session, which is distinct and independent of XRSessions.
  // There will only ever be one in Chrome even when supporting
  // multiple XRSessions.
  internal::ScopedArCoreObject<ArSession*> arcore_session_;
  internal::ScopedArCoreObject<ArFrame*> arcore_frame_;

  // Must be last.
  base::WeakPtrFactory<ArCoreImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreImpl);
};

class ArCoreImplFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_

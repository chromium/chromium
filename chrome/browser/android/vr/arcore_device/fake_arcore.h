// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"

namespace gl {
class GLImageAHardwareBuffer;
}  // namespace gl

namespace device {

// Minimal fake ArCore implementation for testing. It can populate
// the camera texture with a GL_TEXTURE_OES image and do UV transform
// calculations.
class FakeArCore : public ArCore {
 public:
  FakeArCore();
  ~FakeArCore() override;

  // ArCore implementation.
  bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) override;
  void SetCameraTexture(GLuint texture) override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;
  void Pause() override;
  void Resume() override;

  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      const gfx::Size& image_size,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

  void SetCameraAspect(float aspect) { camera_aspect_ = aspect; }

 private:
  bool IsOnGlThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  float camera_aspect_ = 1.0f;
  display::Display::Rotation display_rotation_ =
      display::Display::Rotation::ROTATE_0;
  gfx::Size frame_size_;
  // Storage for the testing placeholder image to keep it alive.
  scoped_refptr<gl::GLImageAHardwareBuffer> placeholder_camera_image_;

  DISALLOW_COPY_AND_ASSIGN(FakeArCore);
};

class FakeArCoreFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_FAKE_ARCORE_H_

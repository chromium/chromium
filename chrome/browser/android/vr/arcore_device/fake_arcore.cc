// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/numerics/math_constants.h"
#include "base/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace device {

FakeArCore::FakeArCore()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeArCore::~FakeArCore() = default;

bool FakeArCore::Initialize(
    base::android::ScopedJavaLocalRef<jobject> application_context) {
  DCHECK(IsOnGlThread());
  return true;
}

void FakeArCore::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());

  display_rotation_ = display_rotation;
  frame_size_ = frame_size;
}

void FakeArCore::SetCameraTexture(GLuint texture) {
  DCHECK(IsOnGlThread());
  // We need a GL_TEXTURE_EXTERNAL_OES to be compatible with the real ArCore.
  // The content doesn't really matter, just create an AHardwareBuffer-backed
  // GLImage and bind it to the texture.

  DVLOG(2) << __FUNCTION__ << ": camera texture=" << texture;

  // Set up a low-resolution image containing the the text "AR" and a shaded
  // background. Goal is to have something asymmetric to make
  // rotations/reflections visible. To test clipping, this uses a 4:3 aspect
  // ratio which is different from typical phone screen aspect ratios.
  gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static int image_width = 12;
  static int image_height = 9;
  // Each value is one column with 8 bits of the result.
  static int image_bitmap[] = {0, 0, 0, 76, 170, 170, 236, 170, 170, 0, 0, 0};

  SetCameraAspect(static_cast<float>(image_width) / image_height);

  // cf. gpu_memory_buffer_impl_android_hardware_buffer
  AHardwareBuffer_Desc desc = {};
  desc.width = image_width;
  desc.height = image_height;
  desc.layers = 1;  // number of images
  desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
               AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
               AHARDWAREBUFFER_USAGE_CPU_READ_RARELY |
               AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      break;
    case gfx::BufferFormat::RGBX_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
      break;
    default:
      NOTREACHED();
  }

  AHardwareBuffer* buffer = nullptr;
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
  DCHECK(buffer);

  uint8_t* data = nullptr;
  int lock_result = base::AndroidHardwareBufferCompat::GetInstance().Lock(
      buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr,
      reinterpret_cast<void**>(&data));
  DCHECK_EQ(lock_result, 0);
  DCHECK(data);

  AHardwareBuffer_Desc desc_locked = {};
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer,
                                                            &desc_locked);
  // The loop checks bits in the input bitmap. If the bit is on, it uses the
  // foreground color - a reddish color, rgb(178, 34, 34).
  // otherwise a background color using a x/y dependent blue/green gradient.
  int stride = desc_locked.stride * 4;  // bytes per pixel;
  for (int y = 0; y < image_height; ++y) {
    for (int x = 0; x < image_width; ++x) {
      bool on = image_bitmap[x] & (1 << y);
      data[y * stride + x * 4] = on ? 178 : 0;
      data[y * stride + x * 4 + 1] = on ? 34 : x * 8;
      data[y * stride + x * 4 + 2] = on ? 34 : y * 8;
      data[y * stride + x * 4 + 3] = 255;
    }
  }

  int unlock_result =
      base::AndroidHardwareBufferCompat::GetInstance().Unlock(buffer, nullptr);
  DCHECK_EQ(unlock_result, 0);

  // Must keep the image alive for the texture to remain valid.
  gfx::Size size(image_width, image_height);
  placeholder_camera_image_ =
      base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size);
  placeholder_camera_image_->Initialize(buffer, true);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  placeholder_camera_image_->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

std::vector<float> FakeArCore::TransformDisplayUvCoords(
    const base::span<const float> uvs) {
  // Try to match ArCore's transfore values.
  //
  // Sample ArCore input: width=1080, height=1795, rotation=0,
  // vecs = (0, 0), (0, 1), (1, 0), (1, 1)
  // Sample ArCore output:
  //   (0.0325544, 1,
  //    0.967446, 1,
  //    0.0325543, 0,
  //    0.967446, 1.19209e-07)
  //
  // FakeArCoreDriver test_arcore;
  // test_arcore.SetCameraAspect(16.f / 9.f);
  // test_arcore.SetDisplayGeometry(0, 1080, 1795);
  // float in[8] = {0, 0, 0, 1, 1, 0, 1, 1};
  // float out[8];
  // test_arcore.TransformDisplayUvCoords(8, &in[0], &out[0]);
  //
  // Fake driver result:
  // TransformDisplayUvCoords: too wide. v0=0.0325521 vrange=0.934896
  //    uv[0]=(0.0325521, 1)
  //    uv[2]=(0.967448, 1)
  //    uv[4]=(0.0325521, 0)
  //    uv[6]=(0.967448, 0)
  //
  // TODO(klausw): move this to a unittest.

  // SetDisplayGeometry should have been called first.
  DCHECK(frame_size_.width());
  DCHECK(frame_size_.height());

  // Do clipping calculations in orientation ROTATE_0. screen U is left=0,
  // right=1. Screen V is bottom=0, top=1. We'll apply screen rotation later.

  float display_aspect =
      static_cast<float>(frame_size_.width()) / frame_size_.height();
  float target_aspect = camera_aspect_;

  // Simulate that the fake camera is rotated by 90 degrees from the usual
  // convention to match the Pixel camera. If the screen is in portrait mode
  // (ROTATION_0), the camera's UV origin is around the bottom right of the
  // screen, with camera +u going left and camera +v going up on the screen. So
  // use the original camera aspect in landscape/seascape, and the inverse
  // for portrait/upside-down orientation.
  if (display_rotation_ == display::Display::Rotation::ROTATE_0 ||
      display_rotation_ == display::Display::Rotation::ROTATE_180) {
    target_aspect = 1.0f / target_aspect;
  }
  DVLOG(3) << __FUNCTION__ << ": rotation=" << display_rotation_
           << " frame_size_.width=" << frame_size_.width()
           << " frame_size_.height=" << frame_size_.height()
           << " display_aspect=" << display_aspect
           << " target_aspect=" << target_aspect;

  float u0 = 0;
  float v0 = 0;
  float urange = 1;
  float vrange = 1;

  if (display_aspect > target_aspect) {
    // Display too wide. Fill width, crop V evenly at top/bottom.
    vrange = target_aspect / display_aspect;
    v0 = (1.f - vrange) / 2;
    DVLOG(3) << ": too wide. v0=" << v0 << " vrange=" << vrange;
  } else {
    // Display too narrow. Fill height, crop U evenly at sides.
    urange = display_aspect / target_aspect;
    u0 = (1.f - urange) / 2;
    DVLOG(3) << ": too narrow. u0=" << u0 << " urange=" << urange;
  }

  size_t num_elements = uvs.size();
  DCHECK(num_elements % 2 == 0);
  std::vector<float> uvs_out;
  uvs_out.reserve(num_elements);
  for (size_t i = 0; i < num_elements; i += 2) {
    float x = uvs[i];
    float y = uvs[i + 1];
    float u, v;
    switch (display_rotation_) {
      case display::Display::Rotation::ROTATE_90:
        u = u0 + x * urange;
        v = v0 + y * vrange;
        break;
      case display::Display::Rotation::ROTATE_180:
        u = 1.f - (v0 + y * vrange);
        v = u0 + x * urange;
        break;
      case display::Display::Rotation::ROTATE_270:
        u = 1.f - (u0 + x * urange);
        v = 1.f - (v0 + y * vrange);
        break;
      case display::Display::Rotation::ROTATE_0:
        u = v0 + y * vrange;
        v = 1.f - (u0 + x * urange);
        break;
    }
    uvs_out.push_back(u);
    uvs_out.push_back(v);
    DVLOG(2) << __FUNCTION__ << ": uv[" << i << "]=(" << u << ", " << v << ")";
  }
  return uvs_out;
}

gfx::Transform FakeArCore::GetProjectionMatrix(float near, float far) {
  DCHECK(IsOnGlThread());
  // Get a projection matrix matching the current screen orientation and
  // aspect. Currently, this uses a hardcoded FOV angle for the smaller screen
  // dimension, and adjusts the other angle to preserve the aspect. A better
  // simulation of ArCore should apply cropping to the underlying fixed-aspect
  // simulated camera image.
  constexpr float fov_half_angle_degrees = 30.f;
  float base_tan = tanf(fov_half_angle_degrees * base::kPiFloat / 180.f);
  float right_tan;
  float up_tan;
  if (display_rotation_ == display::Display::Rotation::ROTATE_0 ||
      display_rotation_ == display::Display::Rotation::ROTATE_180) {
    // portrait aspect
    right_tan = base_tan;
    up_tan = base_tan * frame_size_.height() / frame_size_.width();
  } else {
    // landscape aspect
    up_tan = base_tan;
    right_tan = base_tan * frame_size_.width() / frame_size_.height();
  }
  // Calculate a perspective matrix based on the FOV values.
  gfx::Transform result;
  result.matrix().set(0, 0, 1.f / right_tan);
  result.matrix().set(1, 1, 1.f / up_tan);
  result.matrix().set(2, 2, (near + far) / (near - far));
  result.matrix().set(3, 2, -1.0f);
  result.matrix().set(2, 3, (2.0f * far * near) / (near - far));
  result.matrix().set(3, 3, 0.0f);
  return result;
}

mojom::VRPosePtr FakeArCore::Update(bool* camera_updated) {
  DCHECK(IsOnGlThread());
  DCHECK(camera_updated);

  *camera_updated = true;

  // 1m up from the origin, neutral orientation facing forward.
  mojom::VRPosePtr pose = mojom::VRPose::New();
  pose->orientation.emplace(4);
  pose->position.emplace(3);
  pose->position.value()[0] = 0;
  pose->position.value()[1] = 1;
  pose->position.value()[2] = 0;
  pose->orientation.value()[0] = 0;
  pose->orientation.value()[1] = 0;
  pose->orientation.value()[2] = 0;
  pose->orientation.value()[3] = 1;

  return pose;
}

bool FakeArCore::RequestHitTest(
    const mojom::XRRayPtr& ray,
    const gfx::Size& image_size,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  mojom::XRHitResultPtr hit = mojom::XRHitResult::New();
  hit->hit_matrix.resize(16);
  // Identity matrix - no translation and default orientation.
  hit->hit_matrix.data()[0] = 1;
  hit->hit_matrix.data()[5] = 1;
  hit->hit_matrix.data()[10] = 1;
  hit->hit_matrix.data()[15] = 1;
  hit_results->push_back(std::move(hit));

  return true;
}

void FakeArCore::Pause() {
  DCHECK(IsOnGlThread());
}

void FakeArCore::Resume() {
  DCHECK(IsOnGlThread());
}

bool FakeArCore::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> FakeArCoreFactory::Create() {
  return std::make_unique<FakeArCore>();
}

}  // namespace device

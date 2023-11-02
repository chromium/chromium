// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_UTIL_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_UTIL_H_

#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace gfx {
class Transform;
}  // namespace gfx

// Functions in this file are currently GVR specific functions. If other
// platforms need the same function here, please move it to
// chrome/browser/vr/*util.cc|h and remove dependancy to GVR.
namespace vr {

// Transforms the given gfx::Transform to gvr::Mat4f.
void TransformToGvrMat(const gfx::Transform& in, gvr::Mat4f* out);

// Transforms the given Mat4f to gfx::Transform.
void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out);

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_UTIL_H_

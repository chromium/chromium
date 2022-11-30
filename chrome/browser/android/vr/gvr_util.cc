// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_util.h"

#include "ui/gfx/geometry/transform.h"

namespace vr {

void TransformToGvrMat(const gfx::Transform& in, gvr::Mat4f* out) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out->m[i][j] = in.rc(i, j);
    }
  }
}

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out->set_rc(i, j, in.m[i][j]);
    }
  }
}

}  // namespace vr

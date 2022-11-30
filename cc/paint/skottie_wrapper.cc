// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

namespace cc {

void SkottieWrapper::Seek(float t) {
  Seek(t, FrameDataCallback());
}

}  // namespace cc

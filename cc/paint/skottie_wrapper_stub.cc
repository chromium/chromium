// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include "base/check.h"

namespace cc {

// This stub source file is only built on platforms that don't support skottie.
// Skottie code paths should not be taken at all on these platforms, so
// a concrete SkottieWrapper implementation is not required.

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::UnsafeCreateSerializable(
    std::vector<uint8_t> data) {
  CHECK(false) << "Skottie is not supported on this platform";
  return nullptr;
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::UnsafeCreateNonSerializable(
    base::span<const uint8_t> data) {
  CHECK(false) << "Skottie is not supported on this platform";
  return nullptr;
}

}  // namespace cc

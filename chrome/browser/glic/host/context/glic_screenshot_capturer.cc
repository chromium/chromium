// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_screenshot_capturer_android.h"
#else
#include "chrome/browser/glic/host/context/glic_screenshot_capturer_impl.h"
#endif

namespace glic {

// static
std::unique_ptr<GlicScreenshotCapturer> GlicScreenshotCapturer::Create() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<GlicScreenshotCapturerAndroid>();
#else
  return std::make_unique<GlicScreenshotCapturerImpl>();
#endif
}

}  // namespace glic

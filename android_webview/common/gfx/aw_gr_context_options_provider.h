// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_GFX_AW_GR_CONTEXT_OPTIONS_PROVIDER_H_
#define ANDROID_WEBVIEW_COMMON_GFX_AW_GR_CONTEXT_OPTIONS_PROVIDER_H_

#include "gpu/command_buffer/service/shared_context_state.h"

namespace android_webview {

// Used by gfx to set custom GrContextOptions which get passed to skia. These
// options control how content is rendered on screen.
class AwGrContextOptionsProvider
    : public gpu::SharedContextState::GrContextOptionsProvider {
 public:
  AwGrContextOptionsProvider() = default;
  virtual ~AwGrContextOptionsProvider() = default;

  void SetCustomGrContextOptions(GrContextOptions& options) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_GFX_AW_GR_CONTEXT_OPTIONS_PROVIDER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/gfx/aw_gr_context_options_provider.h"

#include "android_webview/common/aw_features.h"

namespace android_webview {

void AwGrContextOptionsProvider::SetCustomGrContextOptions(
    GrContextOptions& options) const {
  if (base::FeatureList::IsEnabled(
          features::kWebViewDisableSharpeningAndMSAA)) {
    // crbug.com/364872963
    options.fInternalMultisampleCount = 0;
    options.fSharpenMipmappedTextures = false;
  }
}

}  // namespace android_webview

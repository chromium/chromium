// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file only contains the definition of
// AwFieldTrials::EnableRuntimeMutableFeatures(). It lives in its own file
// (rather than in aw_field_trials.cc) so that it can have dedicated OWNERS.
// See //components/variations/RUNTIME_MUTABLE_FEATURES_OWNERS.

#include "android_webview/browser/aw_field_trials.h"
#include "base/feature_list.h"
#include "components/variations/platform_field_trials.h"

void AwFieldTrials::EnableRuntimeMutableFeatures(
    base::FeatureList* feature_list) {
  variations::PlatformFieldTrials::EnableRuntimeMutableFeatures(feature_list);

  // WARNING: WebView does not currently support runtime mutability. Features
  // enabled here will NOT actually be mutated at runtime.
}

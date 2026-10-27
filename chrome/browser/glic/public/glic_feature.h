// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_FEATURE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_FEATURE_H_

namespace glic {

// A Glic feature whose availability depends on the profile's Glic provider
// (consumer Gemini or Gemini Enterprise). See
// `GlicProviderSpec::SupportsFeature()`.
enum class GlicFeature {
  // Gemini Live.
  kLive,
  // Sharing images with Glic.
  kShareImage,

  kMaxValue = kShareImage,
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_FEATURE_H_

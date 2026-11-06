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
  // OS-level entry points that live outside of Chrome's windows: the OS
  // launcher (status tray / menu bar icon) and the OS-global hotkey. Profiles
  // whose provider doesn't support these are never selected by those entry
  // points (see `GlicEnabling::IsOsEntryPointEnabledForProfile()`). Their
  // hotkey is instead handled while one of their Chrome windows is focused (see
  // `InstanceIndependentHotkeyManager`), unless another profile has the
  // OS-global hotkey registered, in which case that profile is launched.
  kOsEntryPoints,
  // Auto-opening Glic for PDFs.
  kPdfAutoOpen,
  // The PDF viewer's summarize button and its API.
  kPdfSummarize,
  // The inline cue shown for text selections.
  kSelectionInlineCue,
  // Capturing a page region via the region selection overlay.
  kCaptureRegion,
  // The Gemini step of the first run feature showcase.
  kFirstRunShowcase,
  // Contextual cueing: page-based suggestions to open Glic.
  kContextualCueing,
  // Zero-state suggestions shown in the Glic panel.
  kZeroStateSuggestions,
  // Chrome Skills (chrome://skills and skill invocation from Glic).
  kSkills,
  // Opening Glic from the Indigo page action (chrome/browser/indigo).
  kIndigoOpenGlic,

  kMaxValue = kIndigoOpenGlic,
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_FEATURE_H_

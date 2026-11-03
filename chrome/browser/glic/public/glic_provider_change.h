// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_

namespace glic {

// How the provider that the current configuration resolves to differs from the
// provider latched for a profile. The latch is fixed for the profile's
// lifetime, so a change only takes effect after a relaunch, and a change that
// is reverted before then goes back to `kNone`.
enum class GlicProviderChange {
  kNone,
  // Gemini -> GEiC.
  kGeminiToGeic,
  // GEiC -> Gemini, e.g. the policy was removed or set to an invalid URL.
  kGeicToGemini,
  // GEiC A -> GEiC B, i.e. a different guest URL.
  kGeicToOtherGeic,
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_

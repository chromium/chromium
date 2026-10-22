// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_H_

namespace glic {

// The product that powers Glic for a profile. Providers are mutually exclusive.
enum class GlicProvider {
  // Gemini in Chrome.
  kGemini,
  // Gemini Enterprise in Chrome (GEiC).
  kGeminiEnterprise,
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_H_

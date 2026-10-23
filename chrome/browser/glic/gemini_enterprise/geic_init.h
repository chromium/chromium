// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_INIT_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_INIT_H_

class Profile;

// Per-profile setup for Gemini Enterprise in Chrome (GEiC).
//
// Like geic_enabling.h, this is GLiC-internal; includes are restricted by DEPS.
namespace geic {

// Runs GEiC setup for `profile`. Called once per profile at startup by
// `GlicKeyedService`, rather than by the per-panel
// `GlicGeminiEnterpriseManager`, so that it runs exactly once per profile.
void InitializeForProfile(Profile* profile);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_INIT_H_

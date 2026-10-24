// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FRE_STORE_IMPL_H_
#define CHROME_BROWSER_GLIC_GLIC_FRE_STORE_IMPL_H_

#include <memory>

namespace glic {

class GlicFreStore;

// Creates the FRE stores owned by `GlicProviderSpec` implementations. Other
// code should get the store from `GlicProviderSpec::GetFreStore()`.

// The Gemini (consumer) FRE, stored in `prefs::kGlicCompletedFre`.
std::unique_ptr<GlicFreStore> CreateGeminiFreStore();

// The Gemini Enterprise in Chrome (GEiC) FRE, stored per GE instance (see
// geic_fre.h). Always completed without `features::kGeicFre`.
std::unique_ptr<GlicFreStore> CreateGeminiEnterpriseFreStore();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_FRE_STORE_IMPL_H_

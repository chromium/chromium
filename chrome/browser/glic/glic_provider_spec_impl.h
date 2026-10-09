// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROVIDER_SPEC_IMPL_H_
#define CHROME_BROWSER_GLIC_GLIC_PROVIDER_SPEC_IMPL_H_

#include <memory>

class Profile;

namespace glic {

class GlicProviderSpec;

// Creates the provider spec for `profile`. Calls geic::IsGeicEnabled(),
// which latches the profile's GEiC decision on its first call.
std::unique_ptr<GlicProviderSpec> CreateGlicProviderSpec(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROVIDER_SPEC_IMPL_H_

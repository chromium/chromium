// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_FRE_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_FRE_H_

#include <optional>

#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"

class Profile;

// Helpers for the Gemini Enterprise in Chrome (GEiC) first-run experience
// (FRE). The FRE is hosted by the GE side-panel web app; Chrome stores its
// completion per Chrome profile for each GE instance (guest URL) in
// `prefs::kGeicCompletedFre`, independent of the GE account signed in within
// the side panel.
//
// Like geic_enabling.h, this is GLiC-internal; includes are restricted by DEPS.
namespace geic {

// Returns the GEiC FRE status for `profile`'s GE instance, or std::nullopt if
// the GEiC FRE doesn't apply to `profile`, i.e. `features::kGeicFre` is
// disabled or `profile` is not latched to Gemini Enterprise. Fails closed:
// returns `kNotStarted` if the entry is missing or invalid. Returns
// `kCompleted` with `--geic-always-skip-fre`.
//
// Compare the result with `==` only: `GetFreStatus(p) != kCompleted` is also
// true when the GEiC FRE doesn't apply.
std::optional<prefs::GeicFreStatus> GetFreStatus(Profile* profile);

// Marks the GEiC FRE completed for `profile`'s GE instance. No-op if
// `GetFreStatus()` is std::nullopt.
void CompleteFre(Profile* profile);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_FRE_H_

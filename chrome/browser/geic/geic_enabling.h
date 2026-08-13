// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_ENABLING_H_
#define CHROME_BROWSER_GEIC_GEIC_ENABLING_H_

class Profile;

namespace geic {

namespace switches {

// Enables GEiC button and feature.
inline constexpr char kGeicEnabled[] = "geic-enabled";

}  // namespace switches

// Returns true if GEiC is enabled for the given `profile`.
bool IsGeicEnabled(Profile* profile = nullptr);

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_ENABLING_H_

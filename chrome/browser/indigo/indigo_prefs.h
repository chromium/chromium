// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_PREFS_H_
#define CHROME_BROWSER_INDIGO_INDIGO_PREFS_H_

class PrefRegistrySimple;

namespace indigo::prefs {

// Policy setting (0, 1, or 2, like GenAiDefaultSettings).
inline constexpr char kIndigoPolicy[] = "indigo.policy";

enum Policy {
  kAllowed = 0,
  kAllowedWithoutModelImprovement = 1,
  kDisallowed = 2,
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace indigo::prefs

#endif  // CHROME_BROWSER_INDIGO_INDIGO_PREFS_H_

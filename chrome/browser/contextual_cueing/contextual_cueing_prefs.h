// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PREFS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PREFS_H_

class PrefRegistrySimple;

namespace contextual_cueing {

extern const char kContextualCueingDisabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_PREFS_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_

class PrefRegistrySimple;

namespace downgrade {

void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_PREFS_H_

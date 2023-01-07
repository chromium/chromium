// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PREFS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PREFS_H_

class PrefRegistrySimple;

namespace component_updater {

// Registers local preferences related to the component updater.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PREFS_H_

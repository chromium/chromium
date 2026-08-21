// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_

class PrefService;

namespace enterprise_isolated_mode {

// Returns true if Isolated Mode should replace Incognito mode.
// This is a convenience wrapper around the components/ version that
// automatically provides the channel.
bool IsolatedModeReplacesIncognito(const PrefService& pref_service);

}  // namespace enterprise_isolated_mode

#endif  // CHROME_BROWSER_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_

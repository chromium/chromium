// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SWITCH_UTILS_H_
#define CHROME_BROWSER_LIFETIME_SWITCH_UTILS_H_

#include "base/command_line.h"

namespace switches {

// Remove the keys that we shouldn't pass through during restart.
void RemoveSwitchesForAutostart(base::CommandLine::SwitchMap* switches);

}  // namespace switches

#endif  // CHROME_BROWSER_LIFETIME_SWITCH_UTILS_H_

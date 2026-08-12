// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

#include <string>

#include "components/critical_actions/core/browser/critical_action_types.h"
#include "url/gurl.h"

namespace critical_actions {

// Returns the destination linkout URL for the given critical action based on
// its action type and associated URL.
std::string GetCriticalActionLinkoutUrl(const CriticalActionEntry& action);

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

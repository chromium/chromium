// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

#include <string>

#include "components/critical_actions/core/browser/critical_action_types.h"
#include "url/gurl.h"

namespace critical_actions {

// Returns the destination linkout URL for a critical action of type
// `action_type`, or an empty string if the action type has no linkout
// destination. `page_url` is the URL of the history visit the action is
// associated with; it refines the destination for action types that are
// page-specific. It may be empty, in which case a generic destination is
// returned.
std::string GetCriticalActionLinkoutUrl(ActionType action_type,
                                        const GURL& page_url);

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

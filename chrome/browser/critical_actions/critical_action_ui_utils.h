// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace critical_actions {

// Returns the destination linkout URL for a critical action of type
// `action_type`, or `std::nullopt` if the action type has no linkout
// destination. `page_url` is the URL of the history visit the action is
// associated with; it refines the destination for action types that are
// page-specific. It may be empty, in which case a generic destination is
// returned.
std::optional<std::string> GetCriticalActionLinkoutUrl(ActionType action_type,
                                                       const GURL& page_url);

// Invoked with the result of `OpenCriticalActionConversation()`.
using OpenConversationCallback =
    base::OnceCallback<void(OpenConversationResult)>;

// Looks up `critical_action_id` in `profile`'s `CriticalActionService` and
// opens the associated Gemini conversation in Glic via `GlicLinkoutHandler`,
// on the tab hosting `web_contents`. `web_contents`, if non-null, must belong
// to `profile` (enforced with a `CHECK`, since opening a conversation stored in
// one profile in another profile's tab would leak data across profiles).
// If `web_contents` is null, or is destroyed before the lookup completes, no
// conversation is opened and the result is `kErrorInternal`.
//
// `source` identifies the requesting History UI surface. With
// `kCriticalActionRow`, the conversation is opened at the action's recorded
// turn (if one was stored) when action-level chat linkouts are enabled;
// otherwise the request is served like `kEntryMenu`, which opens the
// conversation at its default position.
//
// Always records `CriticalActions.OpenConversationResult.{source}`. When the
// action is found, `source` is `kCriticalActionRow` and action-level chat
// linkouts are enabled, also records
// `CriticalActions.OpenConversation.HasConversationAnchor`. `callback` is
// invoked exactly once with the result, possibly synchronously.
void OpenCriticalActionConversation(Profile* profile,
                                    content::WebContents* web_contents,
                                    std::string_view critical_action_id,
                                    OpenConversationSource source,
                                    OpenConversationCallback callback);

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_UI_UTILS_H_

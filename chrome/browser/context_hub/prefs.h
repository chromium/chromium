// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_PREFS_H_
#define CHROME_BROWSER_CONTEXT_HUB_PREFS_H_

class PrefRegistrySimple;

namespace context_hub::prefs {

inline constexpr char kContextHubLastAutoTodosGenerationTime[] =
    "context_hub.last_auto_todos_generation_time";

inline constexpr char kContextHubTabContextSyncContainerId[] =
    "context_hub.tab_context_sync.container_id";

// Dictionary of fishfood Topic feedback keyed by topic id. Only populated
// while the `fishfood_feedback` param of kTopics is enabled; cleared on startup
// otherwise. See ContextHubService::SetTopicFeedback().
inline constexpr char kContextHubTopicsFishfoodFeedback[] =
    "context_hub.topics.fishfood_feedback";

// User-configured keyboard shortcut that opens the Memory Banks search
// palette, stored in `ui::Command` accelerator string format (for example,
// "Ctrl+Shift+K" or "Command+Shift+K"). Empty when no shortcut is set, which is
// the default.
inline constexpr char kContextHubBrowseMemoryBanksShortcut[] =
    "context_hub.memory_banks.browse_shortcut";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace context_hub::prefs

#endif  // CHROME_BROWSER_CONTEXT_HUB_PREFS_H_

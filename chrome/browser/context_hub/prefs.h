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

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace context_hub::prefs

#endif  // CHROME_BROWSER_CONTEXT_HUB_PREFS_H_

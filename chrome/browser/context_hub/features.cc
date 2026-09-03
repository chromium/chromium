// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/features.h"

#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"

namespace context_hub::features {

BASE_FEATURE(kContextHub, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutoTodosTimeoutSeconds,
                   &browser::context_hub::mojom::kAutoTodos,
                   "auto_todos_timeout_seconds",
                   base::Seconds(30));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTabBasedTodosInactivityThreshold,
                   &browser::context_hub::mojom::kAutoTodos,
                   "tab_based_todos_inactivity_threshold",
                   base::Hours(1));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kFirstPartyAutoTodosInterval,
                   &browser::context_hub::mojom::kAutoTodos,
                   "first_party_auto_todos_interval",
                   base::Days(1));

BASE_FEATURE_PARAM(size_t,
                   kMaxTodoFeedbackCacheSize,
                   &browser::context_hub::mojom::kAutoTodos,
                   "max_todo_feedback_cache_size",
                   50);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutoTodosCacheTTL,
                   &browser::context_hub::mojom::kAutoTodos,
                   "auto_todos_cache_ttl",
                   base::Days(30));

BASE_FEATURE(kMemoryBanks, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxMemoryBankEntries,
                   &kMemoryBanks,
                   "max_memory_bank_entries",
                   100);

BASE_FEATURE_PARAM(size_t,
                   kMaxMemoryBankChatHistoryTurns,
                   &kMemoryBanks,
                   20);

BASE_FEATURE_PARAM(int,
                   kMaxTabGroups,
                   &browser::context_hub::mojom::kAutoTabGroups,
                   "max_tab_groups",
                   50);

BASE_FEATURE_PARAM(size_t,
                   kMaxTabGroupChatHistoryTurns,
                   &browser::context_hub::mojom::kAutoTabGroups,
                   "max_tab_group_chat_history_turns",
                   20);

BASE_FEATURE(kContextHubDatabaseStorage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextHubTabContextSyncStorage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kSmartSearchTimeout,
                   &browser::context_hub::mojom::kSmartSearch,
                   "smart_search_timeout",
                   base::Seconds(15));

}  // namespace context_hub::features

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_
#define CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace context_hub::features {

// The main feature flag for the Context Hub service. When disabled,
// all Context Hub features and services are turned off.
BASE_DECLARE_FEATURE(kContextHub);

// Overrides the timeout of the Context Memory Service FetchContext call.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kAutoTodosTimeoutSeconds);

// The maximum number of items stored in the todo feedback cache.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxTodoFeedbackCacheSize);

// The feature flag for the Memory Banks feature in Context Hub.
BASE_DECLARE_FEATURE(kMemoryBanks);

// The maximum number of entries to keep in Memory Banks storage.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxMemoryBankEntries);

// The maximum number of tab groups stored in the in-memory tab group store.
BASE_DECLARE_FEATURE_PARAM(int, kMaxTabGroups);

// The maximum number of turns stored in the tab group chat history cache.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxTabGroupChatHistoryTurns);

// The feature flag for using SQLite database storage for Context Hub.
// When disabled, Memory Banks will use in-memory storage if MemoryBanks
// feature is enabled.
BASE_DECLARE_FEATURE(kContextHubDatabaseStorage);

}  // namespace context_hub::features

#endif  // CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_bar_policy_handler.h"

#include "base/values.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

BookmarkBarPolicyHandler::BookmarkBarPolicyHandler()
    : TypeCheckingPolicyHandler(key::kBookmarkBarEnabled,
                                base::Value::Type::BOOLEAN) {}

BookmarkBarPolicyHandler::~BookmarkBarPolicyHandler() = default;

void BookmarkBarPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value) {
    bool enabled = value->GetBool();
    prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, enabled);
    prefs->SetInteger(
        bookmarks::prefs::kBookmarkBarVisibilityState,
        static_cast<int>(
            enabled ? bookmarks::BookmarkBarVisibilityState::kAlwaysShow
                    : bookmarks::BookmarkBarVisibilityState::kAlwaysHide));
  }
}

}  // namespace policy

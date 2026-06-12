// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_POLICY_HANDLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the |kBookmarkBarEnabled| policy. Maps the boolean policy to both
// |kShowBookmarkBar| and |kBookmarkBarVisibilityState|.
class BookmarkBarPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  BookmarkBarPolicyHandler();
  BookmarkBarPolicyHandler(const BookmarkBarPolicyHandler&) = delete;
  BookmarkBarPolicyHandler& operator=(const BookmarkBarPolicyHandler&) = delete;
  ~BookmarkBarPolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_POLICY_HANDLER_H_

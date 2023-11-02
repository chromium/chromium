// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

// SessionRestoreDelegate is responsible for creating the tab loader and the
// stats collector.
class SessionRestoreDelegate {
 public:
  class RestoredTab {
   public:
    RestoredTab(content::WebContents* contents,
                bool is_active,
                bool is_app,
                bool is_pinned,
                const absl::optional<tab_groups::TabGroupId>& group);
    RestoredTab(const RestoredTab&);
    RestoredTab& operator=(const RestoredTab&);

    ~RestoredTab();

    bool operator<(const RestoredTab& right) const;

    content::WebContents* contents() const { return contents_; }
    bool is_active() const { return is_active_; }
    bool is_app() const { return is_app_; }
    bool is_internal_page() const { return is_internal_page_; }
    bool is_pinned() const { return is_pinned_; }
    const absl::optional<tab_groups::TabGroupId>& group() const {
      return group_;
    }

   private:
    raw_ptr<content::WebContents> contents_;
    bool is_active_;
    bool is_app_;            // Browser window is an app.
    bool is_internal_page_;  // Internal web UI page, like NTP or Settings.
    bool is_pinned_;
    // The ID for the tab group that this tab belonged to, if any. See
    // |TabStripModel::AddToNewGroup()| for more documentation.
    absl::optional<tab_groups::TabGroupId> group_;
  };

  SessionRestoreDelegate() = delete;
  SessionRestoreDelegate(const SessionRestoreDelegate&) = delete;
  SessionRestoreDelegate& operator=(const SessionRestoreDelegate&) = delete;

  static void RestoreTabs(const std::vector<RestoredTab>& tabs,
                          const base::TimeTicks& restore_started);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_

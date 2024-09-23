// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents_observer.h"

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
                const std::optional<tab_groups::TabGroupId>& group);
    RestoredTab(const RestoredTab&);
    RestoredTab& operator=(const RestoredTab&);

    ~RestoredTab();

    bool operator<(const RestoredTab& right) const;

    content::WebContents* contents() const { return contents_.get(); }
    bool is_active() const { return is_active_; }
    bool is_app() const { return is_app_; }
    bool is_internal_page() const { return is_internal_page_; }
    bool is_pinned() const { return is_pinned_; }
    const std::optional<tab_groups::TabGroupId>& group() const {
      return group_;
    }

   private:
    // During restore it's possible for some WebContents to be deleted, which
    // is why this is a WeakPtr. Before SessionRestore calls to RestoreTabs()
    // any RestoredTabs with a deleted WebContents are removed. In other words,
    // when RestoreTabs() is called, the WebContents will be valid.
    base::WeakPtr<content::WebContents> contents_;
    bool is_active_;
    bool is_app_;            // Browser window is an app.
    bool is_internal_page_;  // Internal web UI page, like NTP or Settings.
    bool is_pinned_;
    // The ID for the tab group that this tab belonged to, if any. See
    // |TabStripModel::AddToNewGroup()| for more documentation.
    std::optional<tab_groups::TabGroupId> group_;
  };

  SessionRestoreDelegate() = delete;
  SessionRestoreDelegate(const SessionRestoreDelegate&) = delete;
  SessionRestoreDelegate& operator=(const SessionRestoreDelegate&) = delete;

  static void RestoreTabs(const std::vector<RestoredTab>& tabs,
                          const base::TimeTicks& restore_started);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_

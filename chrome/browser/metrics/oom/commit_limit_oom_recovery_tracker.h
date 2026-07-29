// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_OOM_COMMIT_LIMIT_OOM_RECOVERY_TRACKER_H_
#define CHROME_BROWSER_METRICS_OOM_COMMIT_LIMIT_OOM_RECOVERY_TRACKER_H_

#include "base/callback_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;

namespace tabs {
class TabInterface;
}

// Tracks recovery success rate of tabs that were intentionally terminated
// due to system commit limits.
class CommitLimitOOMRecoveryTracker : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(CommitLimitOOMRecoveryTracker);

  explicit CommitLimitOOMRecoveryTracker(tabs::TabInterface& tab);

  CommitLimitOOMRecoveryTracker(const CommitLimitOOMRecoveryTracker&) = delete;
  CommitLimitOOMRecoveryTracker& operator=(
      const CommitLimitOOMRecoveryTracker&) = delete;

  ~CommitLimitOOMRecoveryTracker() override;

  static CommitLimitOOMRecoveryTracker* From(tabs::TabInterface* tab);

  // Enum matching CommitLimitTerminatedTabReloadResult in stability/enums.xml.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReloadResult {
    kSuccess = 0,
    kFailedOOM = 1,
    kFailedOther = 2,
    kFailedNoDiscard = 3,
    kMaxValue = kFailedNoDiscard,
  };

  // Enum matching CommitLimitTerminatedTabVisibility in stability/enums.xml.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TerminationVisibility {
    kHidden = 0,
    kOccluded = 1,
    kVisible = 2,
    kMaxValue = kVisible,
  };

  enum class TrackingState {
    kIdle,
    kAwaitingReactivation,
    kReloadInFlight,
  };

 private:
  friend class CommitLimitOOMRecoveryTrackerTest;

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Returns if the renderer process exit was due to system commit limits.
  bool IsTerminatedByCommitFailure();

  // Returns true if the navigation URL matches the redirect chain of the last
  // committed navigation entry.
  bool IsReloadNavigation(const GURL& navigation_url);

  void OnTabDiscarded(tabs::TabInterface* tab,
                      content::WebContents* old_contents,
                      content::WebContents* new_contents);

  void TransitionTo(TrackingState new_state);

  ui::ScopedUnownedUserData<CommitLimitOOMRecoveryTracker>
      scoped_unowned_user_data_;

  base::CallbackListSubscription discard_subscription_;

  TrackingState state_ = TrackingState::kIdle;
};

#endif  // CHROME_BROWSER_METRICS_OOM_COMMIT_LIMIT_OOM_RECOVERY_TRACKER_H_

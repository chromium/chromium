// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_

#include <memory>
#include <optional>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// An object that encapsulates session restore policy. For now this is surfaced
// to the TabLoader via TabLoaderDelegate, but eventually TabLoader will be
// merged into TabManager directly.
class SessionRestorePolicy {
 public:
  // Callback that is used by the policy engine to notify its embedder (the
  // TabLoaderDelegate) of changes to tab priorities as they occur. Zero or one
  // score updates may be delivered for each contents that is added; callbacks
  // associated with an explicit WebContents will only be dispatched while the
  // associated WebContents is still being tracked.
  //
  // A callback with a nullptr WebContents is used to indicate that all tabs
  // have received final scores. This is sent at every transition from "not all
  // tabs have final scores" to "all tabs have final scores". This condition is
  // always invalidated at the addition of a new tab, and restored once (a) all
  // unscored tabs have been removed or (b) all unscored tabs receive a final
  // score. This can happen multiple times over the lifetime of a
  // SessionRestorePolicy object.
  using NotifyTabScoreChangedCallback =
      base::RepeatingCallback<void(content::WebContents*, float)>;

  // Used as a testing seam.
  class Delegate;

  SessionRestorePolicy();

  SessionRestorePolicy(const SessionRestorePolicy&) = delete;
  SessionRestorePolicy& operator=(const SessionRestorePolicy&) = delete;

  // Overridden for testing.
  virtual ~SessionRestorePolicy();

  size_t simultaneous_tab_loads() const { return simultaneous_tab_loads_; }

  void SetTabScoreChangedCallback(
      NotifyTabScoreChangedCallback notify_tab_score_changed_callback) {
    notify_tab_score_changed_callback_ = notify_tab_score_changed_callback;
  }

  // Notifies the policy engine of a tab (represented by |contents|) that will
  // be restored. It is expected that the |contents| already be attached to the
  // appropriate tab strip model. This returns an initial restore priority
  // score for the |contents|. The score may change asynchronously via a call
  // to the registered NotifyTabScoreChangedCallback.
  float AddTabForScoring(content::WebContents* contents);

  // Notifies the policy engine of a tab that is no longer to be restored. No
  // score change notifications will be sent for this |contents| after it has
  // been removed.
  void RemoveTabForScoring(content::WebContents* contents);

  // Returns true if the given contents should ever be loaded by
  // session restore. If this returns false then session restore should mark the
  // tab load as deferred and move onto the next tab to restore. Note that this
  // always returns true if the policy logic is disabled.
  //
  // Virtual for testing.
  virtual bool ShouldLoad(content::WebContents* contents) const;

  // Intended to be called by the policy client whenever a tab load has been
  // initiated.
  void NotifyTabLoadStarted();

  // Returns the status of the policy logic.
  bool policy_enabled() const { return policy_enabled_; }

  // Direct access to parameters for testing.
  uint32_t& MinSimultaneousTabLoadsForTesting() {
    return min_simultaneous_tab_loads_;
  }
  uint32_t& MaxSimultaneousTabLoadsForTesting() {
    return max_simultaneous_tab_loads_;
  }
  uint32_t& CoresPerSimultaneousTabLoadForTesting() {
    return cores_per_simultaneous_tab_load_;
  }
  uint32_t& MinTabsToRestoreForTesting() { return min_tabs_to_restore_; }
  uint32_t& MaxTabsToRestoreForTesting() { return max_tabs_to_restore_; }
  uint32_t& MbFreeMemoryPerTabToRestoreForTesting() {
    return mb_free_memory_per_tab_to_restore_;
  }
  base::TimeDelta& MaxTimeSinceLastUseToRestoreForTesting() {
    return max_time_since_last_use_to_restore_;
  }
  uint32_t& MinSiteEngagementToRestoreForTesting() {
    return min_site_engagement_to_restore_;
  }
  size_t& SimultaneousTabLoadsForTesting() { return simultaneous_tab_loads_; }
  void CalculateSimultaneousTabLoadsForTesting() {
    simultaneous_tab_loads_ = CalculateSimultaneousTabLoads();
  }

 protected:
  // Protected so can be exposed for unittesting.

  // Full constructor for testing.
  SessionRestorePolicy(bool policy_enabled, const Delegate* delegate);

  // Helper function for computing the number of loading slots to use. All
  // parameters are exposed for testing.
  static size_t CalculateSimultaneousTabLoads(size_t min_loads,
                                              size_t max_loads,
                                              size_t cores_per_load,
                                              size_t num_cores);

  void SetTabLoadsStartedForTesting(size_t tab_loads_started);

  void UpdateSiteEngagementScoreForTesting(content::WebContents* contents,
                                           size_t score);

 protected:
#if !BUILDFLAG(IS_ANDROID)
  friend class TabDataAccess;
#endif

  // Holds a handful of data about a tab which is used to prioritize it during
  // session restore.
  struct TabData {
    TabData();
    TabData(const TabData&) = delete;
    TabData(TabData&&) = delete;
    ~TabData();

    TabData& operator=(const TabData&) = delete;
    TabData& operator=(TabData&&) = delete;

    // Return true if |used_in_bg| is initialized and set to true, false
    // otherwise.
    bool UsedInBg() const;

    // Indicates whether or not the tab communicates with the user even when it
    // is in the background (tab title changes, favicons, etc).
    // It is initialized to nullopt and set asynchronously to the proper value.
    std::optional<bool> used_in_bg;

    // Indicates whether or not the tab has been pinned by the user. Only
    // applicable on desktop platforms.
    bool is_pinned = false;

    // Indicates whether or not the tab corresponds to a Chrome app (these are
    // deprecated but we still support them for now). Only applicable on
    // desktop platforms.
    bool is_app = false;

    // Indicates whether or not the tab corresponds to an internal chrome://
    // URL. These are considered lower priority for restoring as they can be
    // created locally and usually offline, and have very low latency.
    bool is_internal = false;

    // The site engagement score associated with the tab. Higher values are for
    // sites that see more engagement.
    size_t site_engagement = 0;

    // How long ago since the tab was last made the active tab by the user. This
    // may actually be negative, as it is calculated as a difference between an
    // arbitrary notion of "now" and some time in the past. Lower values still
    // correspond to more recently used and usually more important tabs.
    base::TimeDelta last_active;

    // A higher value here means the tab has higher priority for restoring. This
    // is calculated based on the values of the above properties, which may
    // change as new data becomes available.
    float score = 0.0f;

    struct SiteDataReaderData {
      bool updates_favicon_in_bg = false;
      bool updates_title_in_bg = false;
    };

    // Cancelable callback used to cancel the async initialization of the
    // |used_in_bg| bit.
    base::CancelableOnceCallback<void(SiteDataReaderData)>
        used_in_bg_setter_cancel_callback;
  };

  // This is safe to call from the constructor if |delegate_| is already
  // initialized.
  size_t CalculateSimultaneousTabLoads() const;

  // Posts a task to invoke "NotifyAllTabsScored".
  void DispatchNotifyAllTabsScoredIfNeeded();

  // Invokes the |notify_tab_scored_callback_| with a nullptr WebContents,
  // notifying the embedder that all tabs have final scores.
  void NotifyAllTabsScored();

  // This is a testing seam. By default it immediately redirects to ScoreTab.
  // This should return true if the score has changed, false otherwise.
  virtual bool RescoreTabAfterDataLoaded(content::WebContents* contents,
                                         TabData* tab_data);

  // Calculates a |score| for the given tab. Returns true if it has changed from
  // the existing score.
  static bool ScoreTab(TabData* tab_data);

  // Calculates a score for the "age" of the tab. This is a value between 0
  // (inclusive) and 1 (exclusive), where higher values are attributed to newer
  // tabs.
  static float CalculateAgeScore(const TabData* tab_data);

  // Returns true if given tab has had a final score calculated for it.
  static bool HasFinalScore(const TabData* tab_data);

  // Only used in testing to disable the policy.
  const bool policy_enabled_;

  // Delegate for interface with the system. This allows easy testing of only
  // the logic in this class.
  const raw_ptr<const Delegate> delegate_;

  // The minimum number of tabs to ever load simultaneously. This can be
  // exceeded by user actions or load timeouts. See TabLoader for details.
  uint32_t min_simultaneous_tab_loads_ = 1;

  // The maximum number of simultaneous tab loads that should be permitted.
  // Setting to zero means no maximum is applied.
  uint32_t max_simultaneous_tab_loads_ = 4;

  // The number of CPU cores required before per permitted simultaneous tab
  // load. Setting to zero means no CPU core limit applies.
  uint32_t cores_per_simultaneous_tab_load_ = 2;

  // The minimum total number of tabs to restore (if there are even that many).
  uint32_t min_tabs_to_restore_ = 4;

  // The maximum total number of tabs to restore in a session restore. Setting
  // to zero means no maximum is applied.
  uint32_t max_tabs_to_restore_ = 20;

  // The required amount of system free memory per tab to restore. Setting to
  // zero means no memory limit will be applied.
  uint32_t mb_free_memory_per_tab_to_restore_ = 150;

  // The maximum time since last use of a tab in order for it to be restored.
  // Setting to zero means this logic does not apply.
  base::TimeDelta max_time_since_last_use_to_restore_ = base::Days(30);

  // The minimum site engagement score in order for a tab to be restored.
  // Setting this to zero means all tabs will be restored regardless of the
  // site engagement score.
  uint32_t min_site_engagement_to_restore_ = 15;

  // The number of simultaneous tab loads that are permitted by policy. This
  // is computed based on the number of cores on the machine, except for in
  // tests.
  size_t simultaneous_tab_loads_;

  // The number of tab loads that have started. Every call to ShouldLoad
  // returning to true is assumed to correspond to a tab that starts loading,
  // and increments this value.
  size_t tab_loads_started_ = 0;

  // The number of tabs for which an accurate initial score has been assigned.
  // This is incremented only after the full tab data is available, which
  // may happen asynchronously.
  size_t tabs_scored_ = 0;

  // Used to track the state of the "all tabs scored" notification.
  enum class NotificationState : uint16_t {
    kNotSent = 0,
    kEnRoute = 1,
    kDelivered = 2,
  };
  NotificationState notification_state_ = NotificationState::kDelivered;

  // The collection of tabs being tracked and various data used for scoring
  // them.
  //
  // Note that the value here is a unique_ptr instead of a TabData as this
  // struct isn't movable.
  base::flat_map<content::WebContents*, std::unique_ptr<TabData>> tab_data_;

  // The callback that is invoked in order to update tab restore order.
  NotifyTabScoreChangedCallback notify_tab_score_changed_callback_;

  // The value of "now" to use when calculating time since a tab was used. A
  // constant time is used so that the scores remain constant over the lifetime
  // of a session restore. Note that overlapping session restores may end up
  // having last active times that are *newer* than this value (thus negative
  // ages). CalculateAgeScore can handle this gracefully.
  base::TimeTicks now_;

  // It's possible for this policy object to be destroyed while it has posted
  // notifications in flight. The messages are bound to a weak pointer so that
  // they are not delivered after the policy object is destroyed.
  base::WeakPtrFactory<SessionRestorePolicy> weak_factory_{this};
};

// Abstracts away testing seams for the policy engine. In production code the
// default implementation wraps to base::SysInfo and the
// site_engagement::SiteEngagementService.
class SessionRestorePolicy::Delegate {
 public:
  Delegate();

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  virtual ~Delegate();

  virtual size_t GetNumberOfCores() const = 0;
  virtual size_t GetFreeMemoryMiB() const = 0;
  virtual base::TimeTicks NowTicks() const = 0;
  virtual size_t GetSiteEngagementScore(
      content::WebContents* contents) const = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-forward.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnitBase,
      public TabLifecycleUnitExternal,
      public content::WebContentsObserver {
 public:
  // |observers| is a list of observers to notify when the discarded state or
  // the auto-discardable state of this tab changes. It can be modified outside
  // of this TabLifecycleUnit, but only on the sequence on which this
  // constructor is invoked. |web_contents| and |tab_strip_model| are the
  // WebContents and TabStripModel associated with this tab. The |source| is
  // optional and may be nullptr.
  TabLifecycleUnit(TabLifecycleUnitSource* source,
                   content::WebContents* web_contents,
                   TabStripModel* tab_strip_model);

  TabLifecycleUnit(const TabLifecycleUnit&) = delete;
  TabLifecycleUnit& operator=(const TabLifecycleUnit&) = delete;

  ~TabLifecycleUnit() override;

  // Sets the TabStripModel associated with this tab. The source that created
  // this TabLifecycleUnit is responsible for calling this when the tab is
  // removed from a TabStripModel or inserted into a new TabStripModel.
  void SetTabStripModel(TabStripModel* tab_strip_model);

  // Sets the WebContents associated with this tab. The source that created this
  // TabLifecycleUnit is responsible for calling this when the tab's WebContents
  // changes (e.g. when the tab is discarded or when prerendered or distilled
  // content is displayed).
  void SetWebContents(content::WebContents* web_contents);

  // Invoked when the tab gains or loses focus. This will attempt to load the
  // tab if it was discarded, and focus it if that load was successful.
  void SetFocused(bool focused);

  // Sets the "recently audible" state of this tab. A tab is "recently audible"
  // if a speaker icon is displayed next to it in the tab strip. The source that
  // created this TabLifecycleUnit is responsible for calling this when the
  // "recently audible" state of the tab changes.
  void SetRecentlyAudible(bool recently_audible);

  // Updates the tab's lifecycle state when changed outside the tab
  // lifecycle unit.
  void UpdateLifecycleState(performance_manager::mojom::LifecycleState state);

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::TimeTicks GetLastFocusedTimeTicks() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;
  bool Discard(LifecycleUnitDiscardReason discard_reason,
               uint64_t memory_footprint_estimate) override;

  // TabLifecycleUnitExternal:
  content::WebContents* GetWebContents() const override;
  bool IsAutoDiscardable() const override;
  void SetAutoDiscardable(bool auto_discardable) override;
  bool DiscardTab(mojom::LifecycleUnitDiscardReason reason,
                  uint64_t memory_footprint_estimate) override;
  mojom::LifecycleUnitState GetTabState() const override;

  // LifecycleUnit and TabLifecycleUnitExternal:
  base::Time GetLastFocusedTime() const override;

  base::TimeTicks GetWallTimeWhenHiddenForTesting() const {
    return wall_time_when_hidden_;
  }

  // Attempt to load the tab if it is discarded. Returns whether the load was
  // successful. This doesn't focus the loaded tab.
  bool MaybeLoad();

 protected:
  friend class TabManagerTest;

  // TabLifecycleUnitSource needs to update the state when a external lifecycle
  // state change is observed.
  friend class TabLifecycleUnitSource;

 private:
  void RecomputeLifecycleUnitState();

  // Same as GetSource, but cast to the most derived type.
  TabLifecycleUnitSource* GetTabSource() const;

  // Creates or updates the existing PreDiscardResourceUsage tab helper for the
  // tab's `web_contents` with `discard_reason` and
  // `tab_resident_set_size_estimate`. `tab_resident_set_size_estimate` is in
  // KiB.
  void UpdatePreDiscardResourceUsage(content::WebContents* web_contents,
                                     LifecycleUnitDiscardReason discard_reason,
                                     uint64_t tab_resident_set_size_estimate);

  // Finishes a tab discard, invoked by Discard().
  // `tab_resident_set_size_estimate` is in KiB.
  void FinishDiscard(LifecycleUnitDiscardReason discard_reason,
                     uint64_t tab_resident_set_size_estimate);

  // Finishes a tab discard and preserves the associated web contents. Used only
  // when kWebContentsDiscard is enabled. `tab_resident_set_size_estimate` is in
  // KiB.
  void FinishDiscardAndPreserveWebContents(
      LifecycleUnitDiscardReason discard_reason,
      uint64_t tab_resident_set_size_estimate,
      base::TimeTicks discard_start_time);

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // TabStripModel to which this tab belongs.
  raw_ptr<TabStripModel> tab_strip_model_;

  // Last time ticks at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused. For tabs that aren't currently focused this is
  // initialized using WebContents::GetLastActiveTimeTicks, which causes use
  // times from previous browsing sessions to persist across session restore
  // events.
  // TODO(chrisha): Migrate |last_active_time| to actually track focus time,
  // instead of the time that focus was lost. This is a more meaninful number
  // for all of the clients of |last_active_time|.
  base::TimeTicks last_focused_time_ticks_;

  // Last time ticks at which this tab was focused, or Time::Max() if it is
  // currently focused. For tabs that aren't currently focused this is
  // initialized using WebContents::GetLastActiveTime, which causes use times
  // from previous browsing sessions to persist across session restore
  // events.
  base::Time last_focused_time_;

  // When this is false, CanDiscard() always returns false.
  bool auto_discardable_ = true;

  // Maintains the most recent LifecycleUnitDiscardReason that was passed into
  // Discard().
  LifecycleUnitDiscardReason discard_reason_ =
      LifecycleUnitDiscardReason::EXTERNAL;

  // TimeTicks::Max() if the tab is currently "recently audible", null
  // TimeTicks() if the tab was never "recently audible", last time at which the
  // tab was "recently audible" otherwise.
  base::TimeTicks recently_audible_time_;

  // The wall time when this LifecycleUnit was last hidden, or TimeDelta::Max()
  // if this LifecycleUnit is currently visible.
  base::TimeTicks wall_time_when_hidden_;

  // `page_lifecycle_state_` is the lifecycle state of the associated `PageNode`
  // (`kFrozen` if all frames are frozen, `kActive` otherwise). `is_discarded_`
  // indicates whether the tab is discarded. Together, these properties fully
  // determine the `LifecycleUnitState`.
  performance_manager::mojom::LifecycleState page_lifecycle_state_ =
      performance_manager::mojom::LifecycleState::kRunning;
  bool is_discarded_ = false;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

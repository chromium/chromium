// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_importance_signals.h"

class TabStripModel;

namespace content {
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace resource_coordinator {

class UsageClock;
class TabLifecycleObserver;

// Time during which backgrounded tabs are protected from urgent discarding
// (not on ChromeOS).
static constexpr base::TimeDelta kBackgroundUrgentProtectionTime =
    base::TimeDelta::FromMinutes(10);

// Time during which a tab cannot be discarded after having played audio.
static constexpr base::TimeDelta kTabAudioProtectionTime =
    base::TimeDelta::FromMinutes(1);

// Timeout after which a tab is proactively discarded if the freeze callback
// hasn't been received.
static constexpr base::TimeDelta kProactiveDiscardFreezeTimeout =
    base::TimeDelta::FromMilliseconds(500);

class TabLifecycleUnitExternalImpl;

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnitBase,
      public content::WebContentsObserver {
 public:
  // |observers| is a list of observers to notify when the discarded state or
  // the auto-discardable state of this tab changes. It can be modified outside
  // of this TabLifecycleUnit, but only on the sequence on which this
  // constructor is invoked. |usage_clock| is a clock that measures Chrome usage
  // time. |web_contents| and |tab_strip_model| are the WebContents and
  // TabStripModel associated with this tab. The |source| is optional and may be
  // nullptr.
  TabLifecycleUnit(
      TabLifecycleUnitSource* source,
      base::ObserverList<TabLifecycleObserver>::Unchecked* observers,
      UsageClock* usage_clock,
      content::WebContents* web_contents,
      TabStripModel* tab_strip_model);
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

  // Invoked when the tab gains or loses focus.
  void SetFocused(bool focused);

  // Sets the "recently audible" state of this tab. A tab is "recently audible"
  // if a speaker icon is displayed next to it in the tab strip. The source that
  // created this TabLifecycleUnit is responsible for calling this when the
  // "recently audible" state of the tab changes.
  void SetRecentlyAudible(bool recently_audible);

  // Updates the tab's lifecycle state when changed outside the tab lifecycle
  // unit.
  void UpdateLifecycleState(mojom::LifecycleState state);

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::string16 GetTitle() const override;
  base::TimeTicks GetLastFocusedTime() const override;
  base::ProcessHandle GetProcessHandle() const override;
  SortKey GetSortKey() const override;
  content::Visibility GetVisibility() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  int GetEstimatedMemoryFreedOnDiscardKB() const override;
  bool CanPurge() const override;
  bool CanFreeze(DecisionDetails* decision_details) const override;
  bool CanDiscard(LifecycleUnitDiscardReason reason,
                  DecisionDetails* decision_details) const override;
  bool Freeze() override;
  bool Unfreeze() override;
  ukm::SourceId GetUkmSourceId() const override;

  // Implementations of some functions from TabLifecycleUnitExternal. These are
  // actually called by an instance of TabLifecycleUnitExternalImpl.
  bool IsMediaTab() const;
  bool IsAutoDiscardable() const;
  void SetAutoDiscardable(bool auto_discardable);

 protected:
  friend class TabManagerTest;

  // TabLifecycleUnitSource needs to update the state when a external lifecycle
  // state change is observed.
  friend class TabLifecycleUnitSource;

 private:
  // Indicates if an intervention (freezing or discarding) is proactive or not.
  enum class InterventionType {
    kProactive,
    kExternalOrUrgent,
  };

  // LifecycleUnitBase:
  bool DiscardImpl(LifecycleUnitDiscardReason discard_reason) override;

  // Same as GetSource, but cast to the most derived type.
  TabLifecycleUnitSource* GetTabSource() const;

  // Determines if the tab is a media tab, and populates an optional
  // |decision_details| with full details.
  bool IsMediaTabImpl(DecisionDetails* decision_details) const;

  // For non-urgent discarding, sends a request for freezing to occur prior to
  // discarding the tab.
  void RequestFreezeForDiscard(LifecycleUnitDiscardReason reason);

  // Finishes a tab discard. For an urgent discard, this is invoked by
  // Discard(). For a proactive or external discard, where the tab is frozen
  // prior to being discarded, this is called by UpdateLifecycleState() once the
  // callback has been received, or by |freeze_timeout_timer_| if the
  // kProactiveDiscardFreezeTimeout timeout has passed without receiving the
  // callback.
  void FinishDiscard(LifecycleUnitDiscardReason discard_reason);

  // Returns the RenderProcessHost associated with this tab.
  content::RenderProcessHost* GetRenderProcessHost() const;

  // Initializes |freeze_timeout_timer_| if not already initialized.
  void EnsureFreezeTimeoutTimerInitialized();

  // LifecycleUnitBase:
  void OnLifecycleUnitStateChanged(
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Indicates if freezing or discarding this tab would be noticeable by the
  // user even if it isn't brought back to the foreground. Populates
  // |decision_details| with full details. If |intervention_type| indicates that
  // this is a proactive intervention then more heuristics will be
  // applied.
  void CheckIfTabIsUsedInBackground(DecisionDetails* decision_details,
                                    InterventionType intervention_type) const;

  // Runs the freezing heuristics checks on this tab and store the decision
  // details in |decision_details|. This doesn't check for potential background
  // feature usage.
  void CanFreezeHeuristicsChecks(DecisionDetails* decision_details) const;

  // Runs the discarding heuristics checks on this tab and store the decision
  // details in |decision_details|. If |intervention_type| indicates that
  // this is a proactive intervention then more heuristics will be
  // applied. This doesn't check for potential background feature usage.
  void CanDiscardHeuristicsChecks(DecisionDetails* decision_details,
                                  LifecycleUnitDiscardReason reason) const;

  // List of observers to notify when the discarded state or the auto-
  // discardable state of this tab changes.
  base::ObserverList<TabLifecycleObserver>::Unchecked* observers_;

  // TabStripModel to which this tab belongs.
  TabStripModel* tab_strip_model_;

  // Last time at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused. For tabs that aren't currently focused this is
  // initialized using WebContents::GetLastActiveTime, which causes use times
  // from previous browsing sessions to persist across session restore
  // events.
  // TODO(chrisha): Migrate |last_active_time| to actually track focus time,
  // instead of the time that focus was lost. This is a more meaninful number
  // for all of the clients of |last_active_time|.
  base::TimeTicks last_focused_time_;

  // When this is false, CanDiscard() always returns false.
  bool auto_discardable_ = true;

  // Timer that ensures that this tab does not wait forever for the callback
  // when it is being frozen.
  std::unique_ptr<base::OneShotTimer> freeze_timeout_timer_;

  // TimeTicks::Max() if the tab is currently "recently audible", null
  // TimeTicks() if the tab was never "recently audible", last time at which the
  // tab was "recently audible" otherwise.
  base::TimeTicks recently_audible_time_;

  std::unique_ptr<TabLifecycleUnitExternalImpl> external_impl_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

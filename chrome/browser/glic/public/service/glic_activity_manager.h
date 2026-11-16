// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_H_

#include <cstddef>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "components/actor/core/task_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Public read/subscribe/dismiss interface for the Profile-scoped manager of
// asynchronous Glic activities (Actor tasks, conversation responses, etc.).
class GlicActivityManager {
 public:
  // Returns the GlicActivityManager instance associated with `profile`, or
  // nullptr if GlicKeyedService or Activity Hub is not available.
  static GlicActivityManager* Get(Profile* profile);

  GlicActivityManager() = default;
  GlicActivityManager(const GlicActivityManager&) = delete;
  GlicActivityManager& operator=(const GlicActivityManager&) = delete;
  virtual ~GlicActivityManager() = default;

  // Map of all tracked activities.
  virtual const absl::flat_hash_map<GlicActivityKey, GlicActivityItem>&
  activities() const = 0;

  // Returns tracked activities visible for `active_tab`'s context (or without
  // active-tab filtering when `active_tab` is null), sorted in canonical
  // display priority order.
  virtual std::vector<const GlicActivityItem*> GetSortedActivities(
      const tabs::TabInterface* active_tab) const = 0;

  // Called by UI controllers (e.g., GlicActivityListBubbleController)
  // immediately before rendering the activity list bubble so tab-dependent
  // fields (`has_tab`, `is_enabled`, and tab-closed subtitles) reflect whether
  // a task's target tab was closed since the last task state transition.
  virtual void RefreshActivities() = 0;

  // Returns the unified activity nudge state.
  virtual GlicActivityNudgeState GetCurrentActivityNudgeState() const = 0;

  // Returns the number of completed or attention-requiring activities whose
  // Glic panel has not yet been seen by the user. Drives the numerical badge
  // on the split button.
  virtual size_t GetNumUnseenActivities() const = 0;

  // Returns true if there is any unseen or in-progress activity.
  virtual bool HasUnseenOrInProgressActivities() const = 0;

  // Register for unified activity nudge changes.
  using ActivityNudgeChangeCallback = base::RepeatingCallback<
      void(bool show_bubble, GlicActivityNudgeState activity_nudge_state)>;
  virtual base::CallbackListSubscription RegisterActivityNudgeStateChange(
      ActivityNudgeChangeCallback callback) = 0;

  // Register for unified activity alert / interruption updates.
  using ActivityAlertCallback =
      base::RepeatingCallback<void(ActivityInterruptionLevel level)>;
  virtual base::CallbackListSubscription RegisterActivityAlertCallback(
      ActivityAlertCallback callback) = 0;

  // Marks an activity as seen by the user (e.g., when the user opens/focuses
  // the Glic panel for this conversation, or clicks its entry in the
  // Activity Hub bubble). Decrements the unseen activity count.
  virtual void MarkActivitySeen(const GlicActivityKey& key) = 0;
  // Marks an actor task row as seen by the user.
  virtual void MarkActivitySeen(actor::TaskId task_id) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_H_

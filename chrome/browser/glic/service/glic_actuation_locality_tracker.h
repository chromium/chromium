// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/common/glic_instance_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace actor {
class ActorTask;
struct TaskSourceInfo;
}  // namespace actor

namespace glic {

class GlicInstanceImpl;

// Extracts the opaque Gemini conversation ID from `source_info` if the task
// originated from a Gemini conversation with a non-empty ID, or `std::nullopt`
// otherwise.
std::optional<std::string> GetGeminiConversationId(
    const actor::TaskSourceInfo& source_info);

// Resolves the live target tab of the most recently started non-completed
// Gemini `ActorTask` whose conversation ID matches `conversation_id` exactly in
// `profile`, or nullptr if `profile` is null, `conversation_id` is empty, or no
// such task has a live controlled tab.
tabs::TabInterface* FindActiveGeminiActuationTab(
    Profile* profile,
    std::string_view conversation_id);

// Tracks local browser actuation locality for a single `GlicInstanceImpl`'s
// registered conversation and notifies subscribers with deduplicated
// `mojom::ActuationLocalityState` updates.
class GlicActuationLocalityTracker : public GlicInstanceObserver {
 public:
  using LocalityChangedCallback =
      base::RepeatingCallback<void(const mojom::ActuationLocalityState&)>;

  // `profile` and `instance` must outlive this tracker.
  GlicActuationLocalityTracker(Profile* profile, GlicInstanceImpl* instance);
  GlicActuationLocalityTracker(const GlicActuationLocalityTracker&) = delete;
  GlicActuationLocalityTracker& operator=(const GlicActuationLocalityTracker&) =
      delete;
  ~GlicActuationLocalityTracker() override;

  // Returns the current actuation locality state for `instance_`'s registered
  // conversation, or `not_local` if no conversation is registered or no active
  // Gemini actuation tab exists.
  mojom::ActuationLocalityStatePtr GetCurrentState() const;

  // Subscribes `callback` to deduplicated actuation locality changes for
  // `instance_`'s registered conversation.
  base::CallbackListSubscription AddLocalityChangedCallback(
      LocalityChangedCallback callback);

  // GlicInstanceObserver:
  void OnInstanceVisibilityChanged(const GlicInstance& instance) override;
  void OnInstanceConversationRegistered(const GlicInstance& instance) override;

  bool IsTrackingForTesting() const {
    return static_cast<bool>(task_state_changed_subscription_) &&
           static_cast<bool>(task_surfaces_changed_subscription_);
  }

 private:
  tabs::TabInterface* ResolveActiveTab() const;
  mojom::ActuationLocalityStatePtr ComputeLocalityState(
      tabs::TabInterface* actuation_tab) const;
  void EnsureActorSubscriptions();
  void StopTracking();
  void OnCallbackRemoved();
  void OnActorTaskChanged(actor::ActorTask& task);
  void OnObservedTabWillDetach(tabs::TabInterface* tab,
                               tabs::TabInterface::DetachReason reason);
  void OnObservedTabDidInsert(tabs::TabInterface* tab);
  void ClearDetachingTabAndRefresh(tabs::TabHandle tab_handle);
  void Refresh();
  void UpdateTabSubscriptions(tabs::TabInterface* tab);

  raw_ptr<Profile> profile_;
  raw_ptr<GlicInstanceImpl> instance_;
  std::string conversation_id_;

  base::RepeatingCallbackList<void(const mojom::ActuationLocalityState&)>
      callbacks_;
  mojom::ActuationLocalityStatePtr last_emitted_state_;
  tabs::TabHandle observed_tab_handle_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::CallbackListSubscription tab_did_insert_subscription_;
  base::flat_set<tabs::TabHandle> detaching_tab_handles_;

  base::ScopedObservation<GlicInstanceImpl, GlicInstanceObserver>
      instance_observation_{this};
  base::CallbackListSubscription task_state_changed_subscription_;
  base::CallbackListSubscription task_surfaces_changed_subscription_;

  base::WeakPtrFactory<GlicActuationLocalityTracker> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_

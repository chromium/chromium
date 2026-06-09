// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance_id.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class BrowserWindowInterface;
namespace views {
class Widget;
}  // namespace views

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class GlicActorTaskManager;
class Host;
class GlicSharingManager;

struct ConversationInfo {
  ConversationInfo();
  ConversationInfo(InstanceId instance_id, std::string title);
  ~ConversationInfo();
  ConversationInfo(const ConversationInfo&);
  ConversationInfo& operator=(const ConversationInfo&);

  InstanceId instance_id;
  std::string title;
};

// Observes the state of the glic panel.
class PanelStateObserver : public base::CheckedObserver {
 public:
  virtual void PanelStateChanged(const mojom::PanelState& panel_state) = 0;
};

// Public interface for one instance of the glic web client.
class GlicInstance {
 public:
  virtual ~GlicInstance();

  virtual bool IsShowing() const = 0;
  virtual bool IsActive() = 0;

  virtual void AddStateObserver(PanelStateObserver* observer) = 0;
  virtual void RemoveStateObserver(PanelStateObserver* observer) = 0;

  // Returns the current panel state.
  virtual mojom::PanelState GetPanelState() = 0;

  // TODO(b/501233062): Remove from the public interface once the existing
  // user has migrated away from the API.
  using DestructionCallback = base::OnceCallback<void(GlicInstance*)>;
  virtual base::CallbackListSubscription RegisterWillBeDestroyed(
      DestructionCallback callback) = 0;

  // Get this instance's Host which manages the chrome://glic WebContents.
  // DEPRECATED - Use specific GlicInstance methods instead.
  virtual Host& host() = 0;

  // Sends additional context to the instance.
  // DEPRECATED: Use the invoke API instead.
  virtual void SendAdditionalContext(mojom::AdditionalContextPtr context) = 0;

  // Focuses the instance if it is active.
  // More specifically, it will focus the active embedder.
  virtual void FocusIfActive() = 0;

  // Notifies the instance that a row in the actor task list bubble was clicked.
  // TODO(b/512866173): Look into migrating this usage to the invoke API.
  virtual void NotifyActorTaskListRowClicked(int32_t task_id) = 0;

  // Register a handler to observe experimental triggering related updates.
  // The callback informs if the registration operations was successful or not.
  virtual void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      base::OnceCallback<void(bool)> success_status_callback) = 0;

  // Gets the window size of the active embedder.
  virtual gfx::Size GetPanelSize() = 0;

  // Gets the invoke target that points at the currently active embedder for the
  // instance. If there is no active embedder, uses fallback_surface.
  virtual Target GetInvokeTarget(Target::Surface fallback_surface) = 0;

  // Get this instance's unique identifier.
  virtual const InstanceId& id() const = 0;

  // Get the current conversation ID for this instance.
  virtual std::optional<std::string> conversation_id() const = 0;

  // Get the current conversation title for this instance.
  virtual std::string conversation_title() const = 0;

  // Returns the timestamp when the instance last became active.
  virtual base::Time GetLastActivationTimestamp() const = 0;

  // Returns the duration since the instance was last active.
  // Returns base::TimeDelta() if the instance is currently active.
  virtual base::TimeDelta GetTimeSinceLastActive() const = 0;

  // Returns the duration since the user last submitted a prompt to a
  // conversation in this instance. Returns base::TimeDelta::Max() if no prompt
  // has been submitted yet.
  virtual base::TimeDelta GetTimeSinceLastPromptSubmission() const = 0;

  virtual GlicActorTaskManager* GetActorTaskManager() = 0;

  // Returns true if the instance is currently performing an actuation task.
  virtual bool IsActuating() const = 0;

  // Cancels ongoing actuation task if one exists.
  virtual void CancelTask() = 0;

  // Exposes basic pinning controls to external Chrome consumers.
  virtual GlicSharingManager* GetSharingManager() = 0;
};

}  // namespace glic

namespace base {

template <>
struct ScopedObservationTraits<glic::GlicInstance, glic::PanelStateObserver> {
  static void AddObserver(glic::GlicInstance* source,
                          glic::PanelStateObserver* observer) {
    source->AddStateObserver(observer);
  }
  static void RemoveObserver(glic::GlicInstance* source,
                             glic::PanelStateObserver* observer) {
    source->RemoveStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_H_

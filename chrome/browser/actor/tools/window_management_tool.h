// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/actor/core/task_id.h"
#include "components/sessions/core/session_id.h"

class BrowserWindowInterface;
class ProfileBrowserCollection;

namespace actor {

// A tool to manage browser windows, e.g. create, close, activate, etc.
class WindowManagementTool : public Tool, public BrowserCollectionObserver {
 public:
  enum class Action {
    kCreate,
    kActivate,
    kClose,
    kEnterFullscreen,
    kExitFullscreen
  };

  // Create constructor
  explicit WindowManagementTool(TaskId task_id, ToolDelegate& tool_delegate);

  // Activate|Close|EnterFullscreen|ExitFullscreen constructor.
  WindowManagementTool(Action action,
                       TaskId task_id,
                       ToolDelegate& tool_delegate,
                       int32_t window_id);

  ~WindowManagementTool() override;

  // actor::Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  void UpdateTaskAfterInvoke(ActorTask& task,
                             mojom::ActionResultPtr result,
                             ToolCallback callback) const override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

 private:
  BrowserWindowInterface* GetTargetBrowser() const;

  // Called when the browser with `window_id_` has closed.
  void OnBrowserDidClose(BrowserWindowInterface* browser);

#if BUILDFLAG(IS_ANDROID)
  // Called once the window requested by kCreate exists, or with nullptr if it
  // couldn't be created.
  void OnBrowserWindowCreated(BrowserWindowInterface* browser);
#endif

  // Finishes the invocation when `browser` next becomes the active window.
  void FinishWhenWindowActive(BrowserWindowInterface& browser);

  void OnInvokeFinished(mojom::ActionResultPtr result);

  Action action_;
  std::optional<int32_t> window_id_;

  // If creating a window, this will be set to the handle of the initial
  // surface.
  ActorSurfaceHandle created_actor_surface_handle_;

  ToolCallback callback_;

  // Subscription to the close event for the BrowserWindowInterface
  // corresponding to `window_id_`.
  base::CallbackListSubscription browser_did_close_subscription_;

  // The window whose activation completes the invocation, if waiting for one.
  std::optional<SessionID> awaited_active_window_id_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  base::WeakPtrFactory<WindowManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_

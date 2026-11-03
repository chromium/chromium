// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/window_management_tool.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "url/url_constants.h"
#endif

namespace actor {

namespace {

// Puts `browser` into or out of fullscreen, as a no-op if it's already in the
// requested state.
mojom::ActionResultPtr SetWindowFullscreen(BrowserWindowInterface& browser,
                                           bool fullscreen) {
#if BUILDFLAG(IS_ANDROID)
  // Android has no browser-level fullscreen. A window is fullscreen exactly
  // when its active tab is in immersive (tab) fullscreen, so drive that state
  // through the tab's WebContentsDelegate, the same path HTML
  // requestFullscreen() takes.
  TabListInterface* tab_list = TabListInterface::From(&browser);
  tabs::TabInterface* tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  if (!contents || !contents->GetDelegate()) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "The target window has no active tab.");
  }
  if (contents->IsFullscreen() != fullscreen) {
    if (fullscreen) {
      contents->GetDelegate()->EnterFullscreenModeForTab(
          contents->GetPrimaryMainFrame(), blink::mojom::FullscreenOptions());
    } else {
      contents->ExitFullscreen(/*will_cause_resize=*/true);
    }
  }
#else
  if (browser.GetWindow()->IsFullscreen() != fullscreen) {
    chrome::ToggleFullscreenMode(&browser);
  }
#endif
  return MakeOkResult();
}

}  // namespace

WindowManagementTool::WindowManagementTool(TaskId task_id,
                                           ToolDelegate& tool_delegate)
    : Tool(task_id, tool_delegate), action_(Action::kCreate) {}

WindowManagementTool::WindowManagementTool(Action action,
                                           TaskId task_id,
                                           ToolDelegate& tool_delegate,
                                           int32_t window_id)
    : Tool(task_id, tool_delegate), action_(action), window_id_(window_id) {
  CHECK(action_ == Action::kActivate || action_ == Action::kClose ||
        action_ == Action::kEnterFullscreen ||
        action_ == Action::kExitFullscreen);
}

WindowManagementTool::~WindowManagementTool() = default;

void WindowManagementTool::Validate(ToolCallback callback) {
  switch (action_) {
    case Action::kCreate:
      break;
    case Action::kActivate:
    case Action::kClose:
    case Action::kEnterFullscreen:
    case Action::kExitFullscreen: {
      CHECK(window_id_.has_value());
      mojom::ActionResultPtr result =
          ValidateWindowId(SessionID::FromSerializedValue(*window_id_));
      if (!IsOk(*result)) {
        std::move(callback).Run(std::move(result));
        return;
      }
      if (action_ == Action::kClose) {
        BrowserWindowInterface* browser = GetTargetBrowser();
        browser_did_close_subscription_ = browser->RegisterBrowserDidClose(
            base::BindRepeating(&WindowManagementTool::OnBrowserDidClose,
                                base::Unretained(this)));
      }
      break;
    }
  }

  std::move(callback).Run(MakeOkResult());
}

void WindowManagementTool::Invoke(ToolCallback callback) {
  // The callback is invoked from observing changes to the
  // BrowserWindowInterface instance.
  callback_ = std::move(callback);

  switch (action_) {
    case Action::kCreate: {
      BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                       &tool_delegate().GetProfile(),
                                       /*from_user_gesture=*/false);
      params.initial_show_state = ::ui::mojom::WindowShowState::kNormal;
#if BUILDFLAG(IS_ANDROID)
      // Window creation launches a new Activity, so the window and its
      // initial tab only exist once the asynchronous callback runs.
      CreateBrowserWindow(
          std::move(params),
          base::BindOnce(&WindowManagementTool::OnBrowserWindowCreated,
                         weak_ptr_factory_.GetWeakPtr()));
#else
      BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
      content::WebContents* web_contents =
          chrome::AddAndReturnTabAt(browser, GURL(url::kAboutBlankURL),
                                    /*index=*/-1, /*foreground=*/true);
      if (!web_contents) {
        OnInvokeFinished(
            MakeResult(mojom::ActionResultCode::kNewTabCreationFailed,
                       /*requires_page_stabilization=*/false,
                       "Failed to create a new tab in new window."));
        return;
      }
      created_actor_surface_handle_ = ActorSurfaceHandle::From(web_contents);
      FinishWhenWindowActive(*browser);
      browser->GetWindow()->Show();
#endif  // BUILDFLAG(IS_ANDROID)
      break;
    }
    case Action::kActivate: {
      BrowserWindowInterface* browser = GetTargetBrowser();
      mojom::ActionResultPtr result = ValidateBrowserWindow(browser);
      if (!IsOk(*result)) {
        OnInvokeFinished(std::move(result));
        return;
      }
      if (!browser->GetWindow()) {
        OnInvokeFinished(MakeResult(mojom::ActionResultCode::kWindowWentAway,
                                    /*requires_page_stabilization=*/false,
                                    "The target window could not be found."));
        return;
      }
      // An already-active window won't report an activation, so finish now.
      // Check before requesting activation: on Android, IsActive() reports
      // the pending state once an activation has been requested.
      if (browser->GetWindow()->IsActive()) {
        OnInvokeFinished(MakeOkResult());
        break;
      }
      FinishWhenWindowActive(*browser);
#if BUILDFLAG(IS_ANDROID)
      // Unlike Show(), Activate() also brings a minimized task to the front.
      browser->GetWindow()->Activate();
#else
      browser->GetWindow()->Show();
#endif
      break;
    }
    case Action::kClose: {
      BrowserWindowInterface* browser = GetTargetBrowser();
      mojom::ActionResultPtr result = ValidateBrowserWindow(browser);
      if (!IsOk(*result)) {
        OnInvokeFinished(std::move(result));
        return;
      }
      if (!browser->GetWindow()) {
        OnInvokeFinished(MakeResult(mojom::ActionResultCode::kWindowWentAway,
                                    /*requires_page_stabilization=*/false,
                                    "The target window could not be found."));
        return;
      }

      browser->GetWindow()->Close();
      break;
    }
    case Action::kEnterFullscreen: {
      BrowserWindowInterface* browser = GetTargetBrowser();
      mojom::ActionResultPtr result = ValidateBrowserWindow(browser);
      if (!IsOk(*result)) {
        OnInvokeFinished(std::move(result));
        return;
      }
      if (!browser->GetWindow()) {
        OnInvokeFinished(MakeResult(mojom::ActionResultCode::kWindowWentAway,
                                    /*requires_page_stabilization=*/false,
                                    "The target window could not be found."));
        return;
      }

      OnInvokeFinished(SetWindowFullscreen(*browser, /*fullscreen=*/true));
      break;
    }
    case Action::kExitFullscreen: {
      BrowserWindowInterface* browser = GetTargetBrowser();
      mojom::ActionResultPtr result = ValidateBrowserWindow(browser);
      if (!IsOk(*result)) {
        OnInvokeFinished(std::move(result));
        return;
      }
      if (!browser->GetWindow()) {
        OnInvokeFinished(MakeResult(mojom::ActionResultCode::kWindowWentAway,
                                    /*requires_page_stabilization=*/false,
                                    "The target window could not be found."));
        return;
      }

      OnInvokeFinished(SetWindowFullscreen(*browser, /*fullscreen=*/false));
      break;
    }
  }
}

std::string WindowManagementTool::DebugString() const {
  return "WindowManagementTool";
}

std::string WindowManagementTool::JournalEvent() const {
  switch (action_) {
    case Action::kCreate:
      return "CreateWindow";
    case Action::kActivate:
      return "ActivateWindow";
    case Action::kClose:
      return "CloseWindow";
    case Action::kEnterFullscreen:
      return "EnterFullscreen";
    case Action::kExitFullscreen:
      return "ExitFullscreen";
  }
}

std::unique_ptr<ObservationDelayController>
WindowManagementTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

void WindowManagementTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                                  ToolCallback callback) const {
  if (action_ == Action::kClose) {
    // If closing a window, ensure all acting tabs in this window are removed
    // from the acting set. In particular, this ensures the task isn't stopped
    // when the acting tab is closed.
    BrowserWindowInterface* browser = GetTargetBrowser();
    TabListInterface* tab_list =
        browser ? TabListInterface::From(browser) : nullptr;
    if (tab_list) {
      for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
        task.RemoveActorSurface(ActorSurfaceHandle::From(tab->GetHandle()));
      }
    }
  }
  std::move(callback).Run(MakeOkResult());
}

void WindowManagementTool::UpdateTaskAfterInvoke(ActorTask& task,
                                                 mojom::ActionResultPtr result,
                                                 ToolCallback callback) const {
  // TODO(crbug.com/420669167): Avoid adding the new window's tab if the task
  // already has an actor surface (tab or headless). This limitation can be
  // removed once acting on multiple surfaces is supported.
  if (action_ == Action::kCreate && task.GetActorSurfaces().empty()) {
    CHECK(!created_actor_surface_handle_.is_null());
    task.AddActorSurface(created_actor_surface_handle_,
                         /*stop_task_on_detach=*/true, std::move(callback));
  } else {
    std::move(callback).Run(std::move(result));
  }
}

BrowserWindowInterface* WindowManagementTool::GetTargetBrowser() const {
  CHECK_NE(action_, Action::kCreate);
  CHECK(window_id_.has_value());
  return FindBrowserWindow(SessionID::FromSerializedValue(*window_id_));
}

void WindowManagementTool::OnBrowserDidClose(BrowserWindowInterface* browser) {
  CHECK(window_id_);
  if (action_ == Action::kClose) {
    OnInvokeFinished(MakeOkResult());
  }
}

#if BUILDFLAG(IS_ANDROID)
void WindowManagementTool::OnBrowserWindowCreated(
    BrowserWindowInterface* browser) {
  if (!browser) {
    OnInvokeFinished(MakeResult(mojom::ActionResultCode::kNewTabCreationFailed,
                                /*requires_page_stabilization=*/false,
                                "Failed to create a new window."));
    return;
  }
  // A fully initialized Android window has its initial tab.
  TabListInterface* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  if (!tab) {
    OnInvokeFinished(MakeResult(mojom::ActionResultCode::kNewTabCreationFailed,
                                /*requires_page_stabilization=*/false,
                                "Failed to create a new tab in new window."));
    return;
  }
  created_actor_surface_handle_ = ActorSurfaceHandle::From(tab->GetHandle());
  // The callback runs while the new Activity is becoming top-resumed, before
  // its activation has been reported through the browser collection, so
  // IsActive() can't be trusted here: finishing on it would leave the
  // collection's activation order (and everything built on it, such as the
  // last active window) still pointing at the previous window. Wait for the
  // activation event unless the collection has already recorded it.
  if (ProfileBrowserCollection::GetForProfile(browser->GetProfile())
          ->GetLastActiveBrowser() == browser) {
    OnInvokeFinished(MakeOkResult());
    return;
  }
  FinishWhenWindowActive(*browser);
}
#endif  // BUILDFLAG(IS_ANDROID)

void WindowManagementTool::FinishWhenWindowActive(
    BrowserWindowInterface& browser) {
  awaited_active_window_id_ = browser.GetSessionID();
  browser_collection_observation_.Observe(
      ProfileBrowserCollection::GetForProfile(browser.GetProfile()));
}

void WindowManagementTool::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (awaited_active_window_id_ == browser->GetSessionID()) {
    OnInvokeFinished(MakeOkResult());
  }
}

void WindowManagementTool::OnInvokeFinished(mojom::ActionResultPtr result) {
  if (callback_) {
    PostResponseTask(std::move(callback_), std::move(result));
  }
  browser_did_close_subscription_ = {};
  browser_collection_observation_.Reset();
  awaited_active_window_id_.reset();
}

ActorSurfaceHandle WindowManagementTool::GetTargetActorSurface() const {
  return ActorSurfaceHandle::Null();
}

}  // namespace actor

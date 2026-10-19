// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/window_management_tool.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
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
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/470475787): Port these actions to Android. Their
    // requests aren't dispatched there yet, so they can't reach this point.
    case Action::kCreate:
    case Action::kActivate:
      NOTREACHED();
#else
    case Action::kCreate: {
      BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                       &tool_delegate().GetProfile(),
                                       /*from_user_gesture=*/false);
      params.initial_show_state = ::ui::mojom::WindowShowState::kNormal;
      BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
      browser_did_become_active_subscription_ =
          browser->RegisterDidBecomeActive(base::BindRepeating(
              &WindowManagementTool::OnBrowserDidBecomeActive,
              base::Unretained(this)));
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
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(web_contents);
      created_tab_handle_ = tab->GetHandle();
      browser->GetWindow()->Show();
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
      browser_did_become_active_subscription_ =
          browser->RegisterDidBecomeActive(base::BindRepeating(
              &WindowManagementTool::OnBrowserDidBecomeActive,
              base::Unretained(this)));
      browser->GetWindow()->Show();
      break;
    }
#endif  // BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/470475787): Port to Android, which has no TabStripModel.
    NOTREACHED();
#else
    BrowserWindowInterface* browser = GetTargetBrowser();
    if (browser) {
      for (tabs::TabInterface* tab : *browser->GetTabStripModel()) {
        task.RemoveTab(tab->GetHandle());
      }
    }
#endif
  }
  std::move(callback).Run(MakeOkResult());
}

void WindowManagementTool::UpdateTaskAfterInvoke(ActorTask& task,
                                                 mojom::ActionResultPtr result,
                                                 ToolCallback callback) const {
  // TODO(crbug.com/420669167): Avoid adding the tab if a tab is already acting.
  // This limitation can be removed once multi-tab is supported. In particular,
  // this is needed because GetTabForObservation assumes only a single tab is
  // acting.
  if (action_ == Action::kCreate && task.GetTabs().empty()) {
    CHECK(created_tab_handle_.has_value());
    task.AddTab(*created_tab_handle_, /*stop_task_on_detach=*/true,
                std::move(callback));
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

void WindowManagementTool::OnBrowserDidBecomeActive(
    BrowserWindowInterface* browser) {
  OnInvokeFinished(MakeOkResult());
}

void WindowManagementTool::OnInvokeFinished(mojom::ActionResultPtr result) {
  if (callback_) {
    PostResponseTask(std::move(callback_), std::move(result));
  }
  browser_did_close_subscription_ = {};
  browser_did_become_active_subscription_ = {};
}

ActorSurfaceHandle WindowManagementTool::GetTargetActorSurface() const {
  return ActorSurfaceHandle::Null();
}

}  // namespace actor

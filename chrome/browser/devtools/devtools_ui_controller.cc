// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_controller.h"

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "ui/views/focus/external_focus_tracker.h"

DEFINE_USER_DATA(DevtoolsUIController);

DevtoolsUIController::DevtoolsUIController(
    BrowserWindowInterface* browser,
    std::vector<ContentsContainerView*> contents_container_views)
    : can_dock_devtools_(browser->GetType() ==
                         BrowserWindowInterface::Type::TYPE_NORMAL),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  for (ContentsContainerView* contents_container_view :
       contents_container_views) {
    devtools_web_view_controllers_[contents_container_view] =
        std::make_unique<DevtoolsWebViewController>(contents_container_view);
  }
}

DevtoolsUIController::~DevtoolsUIController() = default;

DevtoolsUIController* DevtoolsUIController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void DevtoolsUIController::TearDown() {
  devtools_web_view_controllers_.clear();
}

bool DevtoolsUIController::UpdateDevtools(
    content::WebContents* web_contents,
    bool update_devtools_web_contents) {
  auto it = std::ranges::find_if(
      devtools_web_view_controllers_, [&web_contents](const auto& entry) {
        return entry.first->contents_view()->web_contents() == web_contents;
      });
  if (it == devtools_web_view_controllers_.end()) {
    return false;
  }

  DevtoolsWebViewController* controller = it->second.get();
  if (update_devtools_web_contents &&
      !DevToolsWindow::GetInTabWebContents(web_contents, nullptr)) {
    controller->RestoreFocus();
  }

  const bool requires_layout =
      controller->UpdateDevtools(web_contents, update_devtools_web_contents);
  if (requires_layout) {
    ContentsContainerView* container_view = it->first;
    container_view->InvalidateLayout();
  }
  return requires_layout;
}

bool DevtoolsUIController::CanDockDevtools() const {
  return can_dock_devtools_;
}

void DevtoolsUIController::SetDevtoolsScrimVisibility(
    content::WebContents* web_contents,
    bool visible) {
  auto it = std::ranges::find_if(
      devtools_web_view_controllers_, [&web_contents](const auto& entry) {
        return entry.first->contents_view()->web_contents() == web_contents;
      });
  if (it != devtools_web_view_controllers_.end()) {
    ContentsContainerView* container_view = it->first;
    container_view->devtools_scrim_view()->SetVisible(visible);
  }
}

DevtoolsUIController::DevtoolsWebViewController::DevtoolsWebViewController(
    ContentsContainerView* contents_container_view)
    : contents_container_view_(contents_container_view) {
  ContentsWebView* contents_web_view =
      contents_container_view_->contents_view();
  web_contents_attached_subscription_ =
      contents_web_view->AddWebContentsAttachedCallback(
          base::BindRepeating(&DevtoolsWebViewController::OnWebContentsAttached,
                              base::Unretained(this)));
  web_contents_detached_subscription_ =
      contents_web_view->AddWebContentsDetachedCallback(
          base::BindRepeating(&DevtoolsWebViewController::OnWebContentsDetached,
                              base::Unretained(this)));
  OnWebContentsAttached(contents_web_view);
}

DevtoolsUIController::DevtoolsWebViewController::~DevtoolsWebViewController() =
    default;

void DevtoolsUIController::DevtoolsWebViewController::OnWebContentsAttached(
    views::WebView* web_view) {
  UpdateDevtools(contents_container_view_->contents_view()->web_contents(),
                 true);
}

void DevtoolsUIController::DevtoolsWebViewController::OnWebContentsDetached(
    views::WebView* web_view) {
  UpdateDevtools(nullptr, true);
}

bool DevtoolsUIController::DevtoolsWebViewController::UpdateDevtools(
    content::WebContents* web_contents,
    bool update_devtools_web_contents) {
  ContentsWebView* contents_view = contents_container_view_->contents_view();
  views::WebView* devtools_web_view =
      contents_container_view_->devtools_web_view();
  DevToolsContentsResizingStrategy strategy;
  content::WebContents* devtools =
      DevToolsWindow::GetInTabWebContents(web_contents, &strategy);

  // Replace devtools WebContents.
  if (devtools_web_view->web_contents() != devtools &&
      update_devtools_web_contents) {
    if (!devtools_web_view->web_contents() && devtools &&
        !devtools_focus_tracker_.get()) {
      // Install devtools focus tracker when dev tools window is shown for the
      // first time.
      devtools_focus_tracker_ = std::make_unique<views::ExternalFocusTracker>(
          devtools_web_view, contents_container_view_->GetFocusManager());
    }

    // Reset the focus tracker when hiding devtools window.
    if (devtools_web_view->web_contents() && !devtools &&
        devtools_focus_tracker_.get()) {
      devtools_focus_tracker_.reset();
    }
    devtools_web_view->SetWebContents(devtools);
  }

  bool devtools_layout_updated;
  if (devtools) {
    devtools_layout_updated =
        !devtools_web_view->GetVisible() ||
        !contents_container_view_->contents_resizing_strategy().Equals(
            strategy);
    devtools_web_view->SetVisible(true);
    contents_container_view_->SetContentsResizingStrategy(strategy);
  } else {
    devtools_layout_updated =
        devtools_web_view->GetVisible() ||
        !contents_container_view_->contents_resizing_strategy().Equals(
            DevToolsContentsResizingStrategy());
    devtools_web_view->SetVisible(false);
    contents_container_view_->SetContentsResizingStrategy(
        DevToolsContentsResizingStrategy());
  }

  if (devtools) {
    // When strategy.hide_inspected_contents() returns true, we are hiding the
    // WebContents behind the devtools_web_view_. Otherwise, the WebContents
    // should be right above the devtools_web_view_.
    size_t devtools_index =
        contents_container_view_->GetIndexOf(devtools_web_view).value();
    size_t contents_index =
        contents_container_view_->GetIndexOf(contents_view).value();
    bool devtools_is_on_top = devtools_index > contents_index;
    if (strategy.hide_inspected_contents() != devtools_is_on_top) {
      contents_container_view_->ReorderChildView(contents_view, devtools_index);
    }
  }

  return devtools_layout_updated;
}

void DevtoolsUIController::DevtoolsWebViewController::RestoreFocus() {
  if (devtools_focus_tracker_.get()) {
    devtools_focus_tracker_->FocusLastFocusedExternalView();
  }
}

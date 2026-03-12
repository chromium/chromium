// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host_desktop_impl.h"

#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"

namespace {
// Configuration for the desired width of the side panel.
inline constexpr int kSidePanelPreferredDefaultWidth = 440;
}  // namespace

namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksPanelHost> ContextualTasksPanelHost::Create(
    BrowserWindowInterface* browser_window) {
  return std::make_unique<ContextualTasksPanelHostDesktopImpl>(
      browser_window, browser_window->GetFeatures().side_panel_ui());
}

ContextualTasksPanelHostDesktopImpl::ContextualTasksPanelHostDesktopImpl(
    BrowserWindowInterface* browser_window,
    SidePanelUI* side_panel_ui)
    : browser_window_(browser_window), side_panel_ui_(side_panel_ui) {
  CreateAndRegisterEntry();
}

ContextualTasksPanelHostDesktopImpl::~ContextualTasksPanelHostDesktopImpl() {
  SidePanelRegistry* global_registry = SidePanelRegistry::From(browser_window_);
  if (global_registry) {
    auto* contextual_tasks_entry = global_registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
    if (contextual_tasks_entry) {
      contextual_tasks_entry->RemoveObserver(this);
    }
  }
}

void ContextualTasksPanelHostDesktopImpl::AddObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksPanelHostDesktopImpl::RemoveObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTasksPanelHostDesktopImpl::Show(
    ContextualTasksPanelHost::AnimationStyle animation) {
  // Only show the side panel if it's closed.
  if (IsPanelOpenForContextualTask()) {
    return;
  }

  switch (animation) {
    case ContextualTasksPanelHost::AnimationStyle::kStandard:
      side_panel_ui_->Show(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
      break;
    case ContextualTasksPanelHost::AnimationStyle::kTransitionFromTab:
      ShowFromTab();
      break;
    case ContextualTasksPanelHost::AnimationStyle::kNoAnimation:
      side_panel_ui_->Show(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
          /*open_trigger=*/std::nullopt,
          /*suppress_animations=*/true);
      break;
  }
}

void ContextualTasksPanelHostDesktopImpl::Close(
    ContextualTasksPanelHost::AnimationStyle animation) {
  // `kTransitionFromTab` is only supported for showing the panel.
  CHECK_NE(animation,
           ContextualTasksPanelHost::AnimationStyle::kTransitionFromTab);

  side_panel_ui_->Close(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntryHideReason::kSidePanelClosed,
      /*suppress_animations=*/animation ==
          ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
}

bool ContextualTasksPanelHostDesktopImpl::IsPanelInitialized() {
  return web_view_ != nullptr;
}

bool ContextualTasksPanelHostDesktopImpl::IsPanelOpenForContextualTask() const {
  return side_panel_ui_->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

bool ContextualTasksPanelHostDesktopImpl::IsPanelSuppressed() const {
  if (suppressed_for_testing_) {
    return true;
  }
  // If the glic side panel is open, do not override it with the contextual
  // tasks side panel when active tab is changed.
  return side_panel_ui_->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic));
}

void ContextualTasksPanelHostDesktopImpl::SetPanelSuppressedForTesting(
    bool suppressed) {
  suppressed_for_testing_ = suppressed;
}

content::WebContents* ContextualTasksPanelHostDesktopImpl::GetWebContents() {
  if (!IsPanelInitialized()) {
    return nullptr;
  }
  return web_view_->web_contents();
}

void ContextualTasksPanelHostDesktopImpl::SetWebContents(
    content::WebContents* web_contents) {
  if (IsPanelInitialized()) {
    web_view_->SetWebContents(web_contents);
  }
}

void ContextualTasksPanelHostDesktopImpl::OnEntryShown(SidePanelEntry* entry) {
  NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kVisible,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);
}

void ContextualTasksPanelHostDesktopImpl::OnEntryHidden(SidePanelEntry* entry) {
  NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);
}

std::unique_ptr<views::View>
ContextualTasksPanelHostDesktopImpl::CreateSidePanelView(
    SidePanelEntryScope& scope) {
  std::unique_ptr<ContextualTasksWebView> web_view =
      std::make_unique<ContextualTasksWebView>(browser_window_->GetProfile());
  web_view->SetPaintToLayer();
  web_view->layer()->SetFillsBoundsOpaquely(false);
  web_view_ = web_view->GetWeakPtr();

  NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kVisible,
      ContextualTasksPanelHost::StateChangeReason::kSystemAction);

  return web_view;
}

void ContextualTasksPanelHostDesktopImpl::NotifySurfaceStateChanged(
    ContextualTasksPanelHost::SurfaceState state,
    ContextualTasksPanelHost::StateChangeReason reason) {
  observers_.Notify(&ContextualTasksPanelHost::Observer::OnSurfaceStateChanged,
                    state, reason);
}

void ContextualTasksPanelHostDesktopImpl::CreateAndRegisterEntry() {
  auto* global_registry = SidePanelRegistry::From(browser_window_);
  CHECK(global_registry);

  if (global_registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
      base::BindRepeating(
          [](base::WeakPtr<ContextualTasksPanelHostDesktopImpl> host,
             SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
            return host ? host->CreateSidePanelView(scope) : nullptr;
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating([]() { return kSidePanelPreferredDefaultWidth; }));
  entry->set_should_show_ephemerally_in_toolbar(false);
  entry->set_should_show_header(false);
  entry->set_should_show_outline(false);
  global_registry->Register(std::move(entry));

  // Observe the side panel entry.
  auto* registered_entry = global_registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
  registered_entry->AddObserver(this);
}

void ContextualTasksPanelHostDesktopImpl::ShowFromTab() {
  views::View* content = BrowserElementsViews::From(browser_window_)
                             ->RetrieveView(kActiveContentsWebViewRetrievalId);
  gfx::Rect content_bounds_in_browser_coordinates =
      content->ConvertRectToWidget(content->GetContentsBounds());
  side_panel_ui_->ShowFrom(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
      content_bounds_in_browser_coordinates);
}

}  // namespace contextual_tasks

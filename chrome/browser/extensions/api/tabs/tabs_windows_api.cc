// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"

#include <memory>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

TabsWindowsAPI::TabsWindowsAPI(content::BrowserContext* context)
    : browser_context_(context),
      windows_event_router_(
          new WindowsEventRouter(Profile::FromBrowserContext(context))) {
  EventRouter* event_router = EventRouter::Get(browser_context_);

  // Tabs API Events.
  event_router->RegisterObserver(this, api::tabs::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnUpdated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnMoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnSelectionChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActiveChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActivated::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnHighlightChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnHighlighted::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnDetached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnAttached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnRemoved::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnReplaced::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnZoomChange::kEventName);

  // Windows API Events.
  event_router->RegisterObserver(this, api::windows::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::windows::OnRemoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::windows::OnFocusChanged::kEventName);
  event_router->RegisterObserver(this,
                                 api::windows::OnBoundsChanged::kEventName);
}

TabsWindowsAPI::~TabsWindowsAPI() {
}

// static
TabsWindowsAPI* TabsWindowsAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<TabsWindowsAPI>::Get(context);
}

TabsEventRouter* TabsWindowsAPI::tabs_event_router() {
  if (!tabs_event_router_.get()) {
    tabs_event_router_ = std::make_unique<TabsEventRouter>(
        Profile::FromBrowserContext(browser_context_));
  }
  return tabs_event_router_.get();
}

WindowsEventRouter* TabsWindowsAPI::windows_event_router() {
  return windows_event_router_.get();
}

void TabsWindowsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabsWindowsAPI>>::
    DestructorAtExit g_tabs_windows_api_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<TabsWindowsAPI>*
TabsWindowsAPI::GetFactoryInstance() {
  return g_tabs_windows_api_factory.Pointer();
}

void TabsWindowsAPI::OnListenerAdded(const EventListenerInfo& details) {
  // Initialize the event routers.
  tabs_event_router();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions

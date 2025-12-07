// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"

#include <memory>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/tabs/tabs_event_router_android.h"
#else
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

TabsWindowsAPI::TabsWindowsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  windows_event_router_ = std::make_unique<WindowsEventRouter>(
      Profile::FromBrowserContext(browser_context_));
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

TabsWindowsAPI::~TabsWindowsAPI() = default;

// static
TabsWindowsAPI* TabsWindowsAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<TabsWindowsAPI>::Get(context);
}

void TabsWindowsAPI::InitTabsEventRouter() {
#if BUILDFLAG(IS_ANDROID)
  tabs_event_router_android_ = std::make_unique<TabsEventRouterAndroid>(
      Profile::FromBrowserContext(browser_context_));
#else
  tabs_event_router_ = std::make_unique<TabsEventRouter>(
      Profile::FromBrowserContext(browser_context_));
#endif
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/371432155): Delete this method once TabsEventRouter works on
// desktop Android.
TabsEventRouterAndroid* TabsWindowsAPI::tabs_event_router_android() {
  return tabs_event_router_android_.get();
}
#else
TabsEventRouter* TabsWindowsAPI::tabs_event_router() {
  return tabs_event_router_.get();
}
#endif

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
  InitTabsEventRouter();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class TabsEventRouter;
class TabsEventRouterAndroid;
class WindowsEventRouter;

// TabsWindowsAPI is a BrowserContextKeyedAPI that manages the TabsEventRouter
// and WindowsEventRouter. It routes various events to the appropriate event
// listeners in the renderers.
class TabsWindowsAPI : public BrowserContextKeyedAPI,
                       public EventRouter::Observer {
 public:
  explicit TabsWindowsAPI(content::BrowserContext* context);
  ~TabsWindowsAPI() override;

  // Convenience method to get the TabsWindowsAPI for a profile.
  static TabsWindowsAPI* Get(content::BrowserContext* context);

  // Creates the tabs event router. Visible for testing.
  void InitTabsEventRouter();

#if BUILDFLAG(IS_ANDROID)
  TabsEventRouterAndroid* tabs_event_router_android();
#else
  TabsEventRouter* tabs_event_router();
#endif

  WindowsEventRouter* windows_event_router();

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TabsWindowsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<TabsWindowsAPI>;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "TabsWindowsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/427503497): Remove this once TabsEventRouter works on
  // Android.
  std::unique_ptr<TabsEventRouterAndroid> tabs_event_router_android_;
#else
  std::unique_ptr<TabsEventRouter> tabs_event_router_;
#endif
  std::unique_ptr<WindowsEventRouter> windows_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_

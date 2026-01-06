// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <set>
#include <string>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_android.h"
#else
#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_non_android.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
// TODO(https://crbug.com/473593117): Right now, the entire functionality of
// this class is essentially delegated to its platform delegate. We need to
// pull this functionality into this class.
class TabsEventRouter {
 public:
  explicit TabsEventRouter(Profile* profile);
  TabsEventRouter(const TabsEventRouter&) = delete;
  TabsEventRouter& operator=(const TabsEventRouter&) = delete;
  ~TabsEventRouter();

 private:
  // The platform delegate is basically a platform-specific addendum to this
  // class, so we allow it to reach into this class's internal state.
  friend class TabsEventRouterPlatformDelegate;

  // Packages `changed_property_names` as a tab updated event for the tab
  // `contents` and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               std::set<std::string> changed_property_names);

  TabsEventRouterPlatformDelegate platform_delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

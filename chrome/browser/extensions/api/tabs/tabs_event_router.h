// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/zoom/zoom_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

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
class TabsEventRouter : public favicon::FaviconDriverObserver,
                        public zoom::ZoomObserver {
 public:
  explicit TabsEventRouter(Profile* profile);
  TabsEventRouter(const TabsEventRouter&) = delete;
  TabsEventRouter& operator=(const TabsEventRouter&) = delete;
  ~TabsEventRouter() override;

 private:
  // The platform delegate is basically a platform-specific addendum to this
  // class, so we allow it to reach into this class's internal state.
  friend class TabsEventRouterPlatformDelegate;

  // Registers to receive the various notifications we are interested in for a
  // tab.
  void RegisterForTabNotifications(content::WebContents& contents);

  // Removes notifications and tab entry added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents& contents);

  // Packages `changed_property_names` as a tab updated event for the tab
  // `contents` and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               std::set<std::string> changed_property_names);

  // Dispatches the `tabs.onCreated` API event for the given `contents`.
  // `active` indicates if the tab is active in its tab strip.
  void DispatchTabCreatedEvent(content::WebContents* contents, bool active);

  // The DispatchEvent methods forward events to the `profile`'s event router.
  // The TabsEventRouterPlatformDelegate listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args,
                     EventRouter::UserGestureState user_gesture);

  // ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // Triggers a tab updated event if the favicon URL changes.
  void FaviconUrlUpdated(content::WebContents* contents);

  // Observations for different state changes in tabs.
  base::ScopedMultiSourceObservation<favicon::FaviconDriver,
                                     favicon::FaviconDriverObserver>
      favicon_scoped_observations_{this};
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_scoped_observations_{this};

  // The profile this router is associated with.
  raw_ptr<Profile> profile_;

  TabsEventRouterPlatformDelegate platform_delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

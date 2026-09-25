// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ash::app_time {

// Observes web contents for navigation events and notifies listeners.
class WebTimeNavigationObserver : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(WebTimeNavigationObserver);

  struct NavigationInfo {
    base::Time navigation_finish_time;

    // Boolean to specify if the navigation ended in an error page.
    bool is_error;

    // Boolean to specify if the WebContents is hosting a web app.
    bool is_web_app;

    // The url that is being hosted in WebContents.
    GURL url;

    // The WebContent where the navigation has taken place.
    raw_ptr<content::WebContents> web_contents;
  };

  class EventListener : public base::CheckedObserver {
   public:
    virtual void OnWebActivityChanged(const NavigationInfo& info) {}
    virtual void WebTimeNavigationObserverDestroyed(
        WebTimeNavigationObserver* observer) {}
  };

  static std::unique_ptr<WebTimeNavigationObserver> MaybeCreate(
      tabs::TabInterface& tab,
      content::WebContents* web_contents);

  static WebTimeNavigationObserver* From(tabs::TabInterface* tab);
  static WebTimeNavigationObserver* FromWebContents(
      content::WebContents* web_contents);
  static const WebTimeNavigationObserver* FromWebContents(
      const content::WebContents* web_contents);

  WebTimeNavigationObserver(const WebTimeNavigationObserver&) = delete;
  WebTimeNavigationObserver& operator=(const WebTimeNavigationObserver&) =
      delete;
  ~WebTimeNavigationObserver() override;

  void OnDiscardContents(content::WebContents* new_contents);

  void AddObserver(EventListener* listener);
  void RemoveObserver(EventListener* listener);

  bool IsWebApp() const;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  WebTimeNavigationObserver(tabs::TabInterface& tab,
                            content::WebContents* web_contents);

  base::ObserverList<EventListener> listeners_;

  ui::ScopedUnownedUserData<WebTimeNavigationObserver>
      scoped_unowned_user_data_;
};

}  // namespace ash::app_time

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_

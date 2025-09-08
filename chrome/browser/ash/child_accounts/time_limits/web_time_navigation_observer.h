// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;

namespace ash::app_time {

// Observes web contents for navigation events and notifies listeners.
class WebTimeNavigationObserver
    : public content::WebContentsUserData<WebTimeNavigationObserver>,
      public content::WebContentsObserver {
 public:
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

  static void MaybeCreateForWebContents(content::WebContents* web_contents);
  static void CreateForWebContents(content::WebContents* web_contents) = delete;

  WebTimeNavigationObserver(const WebTimeNavigationObserver&) = delete;
  WebTimeNavigationObserver& operator=(const WebTimeNavigationObserver&) =
      delete;
  ~WebTimeNavigationObserver() override;

  void AddObserver(EventListener* listener);
  void RemoveObserver(EventListener* listener);

  bool IsWebApp() const;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<WebTimeNavigationObserver>;

  explicit WebTimeNavigationObserver(content::WebContents* web_contents);

  base::ObserverList<EventListener> listeners_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ash::app_time

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_NAVIGATION_OBSERVER_H_

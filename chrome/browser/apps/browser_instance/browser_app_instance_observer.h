// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_OBSERVER_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_OBSERVER_H_

namespace apps {

struct BrowserAppInstance;
struct BrowserWindowInstance;

// Observer interface to listen to |BrowserAppInstanceTracker| events.
class BrowserAppInstanceObserver {
 public:
  virtual ~BrowserAppInstanceObserver();

  // Called when a new non-app browser window is created.
  virtual void OnBrowserWindowAdded(const BrowserWindowInstance& instance);

  // Called when a non-app browser window is updated.
  virtual void OnBrowserWindowUpdated(const BrowserWindowInstance& instance);

  // Called when a non-app browser window is closed.
  virtual void OnBrowserWindowRemoved(const BrowserWindowInstance& instance);

  // Called when a new app instance is started: a tab or a window is open, a tab
  // navigates to a URL with an app.
  virtual void OnBrowserAppAdded(const BrowserAppInstance& instance);

  // Called when the app's window, app's tab, or a window containing the app's
  // tab changes properties, or a tab gets moved between browsers.
  virtual void OnBrowserAppUpdated(const BrowserAppInstance& instance);

  // Called when an app instance is stopped: a tab or window is closed, a new
  // windowed app is open, a tab navigates to a URL with an app.
  virtual void OnBrowserAppRemoved(const BrowserAppInstance& instance);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_OBSERVER_H_

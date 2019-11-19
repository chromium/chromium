// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_
#define CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_

#include "base/android/jni_android.h"
#include "base/callback_forward.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/singleton.h"

namespace content {
class BrowserContext;
struct OpenURLParams;
class WebContents;
}

// Launcher for creating new tabs on Android from a background service, where
// there may not necessarily be an Activity or a tab model at all. When the
// tab has been launched, the user of this class will be informed with the
// content::WebContents instance associated with the tab.
class ServiceTabLauncher {
  using TabLaunchedCallback = base::OnceCallback<void(content::WebContents*)>;

 public:
  // Returns the singleton instance of the service tab launcher.
  static ServiceTabLauncher* GetInstance();

  // Launches a new tab when we're in a Service rather than in an Activity.
  // |callback| will be invoked with the resulting content::WebContents* when
  // the tab is avialable. This method must only be called from the UI thread.
  void LaunchTab(content::BrowserContext* browser_context,
                 const content::OpenURLParams& params,
                 TabLaunchedCallback callback);

  // To be called when the tab for |request_id| has launched, with the
  // associated |web_contents|. The WebContents must not yet have started
  // the provisional load for the main frame of the navigation.
  void OnTabLaunched(int request_id, content::WebContents* web_contents);

 private:
  friend struct base::DefaultSingletonTraits<ServiceTabLauncher>;

  ServiceTabLauncher();
  ~ServiceTabLauncher();

  base::IDMap<std::unique_ptr<TabLaunchedCallback>> tab_launched_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ServiceTabLauncher);
};

#endif  // CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_

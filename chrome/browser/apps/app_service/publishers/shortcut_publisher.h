// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// ShortcutPublisher parent class (in the App Service sense) for all shortcut
// publishers. See components/services/app_service/README.md.
class ShortcutPublisher {
 public:
  explicit ShortcutPublisher(AppServiceProxy* proxy);
  ShortcutPublisher(const ShortcutPublisher&) = delete;
  ShortcutPublisher& operator=(const ShortcutPublisher&) = delete;
  virtual ~ShortcutPublisher();

  // Registers this ShortcutPublisher to AppServiceProxy, allowing it to receive
  // App Service API calls. This function must be called after the object's
  // creation, and can't be called in the constructor function to avoid
  // receiving API calls before being fully constructed and ready. This should
  // be called immediately before the first call to ShortcutPublisher::Publish
  // that sends the initial list of apps to the App Service.
  void RegisterShortcutPublisher(AppType app_type);

 protected:
  // Publish one `delta` to AppServiceProxy. Should be called whenever the
  // shortcut represented by `delta` undergoes some state change to inform
  // AppServiceProxy of the change. Ensure that RegisterShortcutPublisher() has
  // been called before the first call to this method.
  void PublishShortcut(ShortcutPtr delta);

  AppServiceProxy* proxy() { return proxy_; }

 private:
  const raw_ptr<AppServiceProxy> proxy_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_

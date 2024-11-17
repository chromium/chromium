// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace chrome_apps {

// A helper class to observe application termination, and notify the
// appropriate apps systems.
class AppTerminationObserver : public KeyedService {
 public:
  explicit AppTerminationObserver(content::BrowserContext* browser_context);
  AppTerminationObserver(const AppTerminationObserver&) = delete;
  AppTerminationObserver& operator=(const AppTerminationObserver&) = delete;
  ~AppTerminationObserver() override;

  // KeyedService:
  void Shutdown() override;

 private:
  void OnAppTerminating();

  raw_ptr<content::BrowserContext> browser_context_;

  std::optional<base::CallbackListSubscription> subscription_;
};

}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_

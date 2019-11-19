// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_BROWSER_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_BROWSER_API_H_

#include "extensions/browser/extension_function.h"

namespace chrome_apps {
namespace api {

class BrowserOpenTabFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browser.openTab", BROWSER_OPENTAB)

 protected:
  ~BrowserOpenTabFunction() override;

  ResponseAction Run() override;
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_BROWSER_API_H_

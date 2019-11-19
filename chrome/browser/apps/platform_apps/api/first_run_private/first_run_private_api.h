// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_FIRST_RUN_PRIVATE_FIRST_RUN_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_FIRST_RUN_PRIVATE_FIRST_RUN_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/common/apps/platform_apps/api/first_run_private.h"
#include "extensions/browser/extension_function.h"

namespace chrome_apps {
namespace api {

class FirstRunPrivateGetLocalizedStringsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("firstRunPrivate.getLocalizedStrings",
                             FIRSTRUNPRIVATE_GETLOCALIZEDSTRINGS)

 protected:
  ~FirstRunPrivateGetLocalizedStringsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class FirstRunPrivateLaunchTutorialFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("firstRunPrivate.launchTutorial",
                             FIRSTRUNPRIVATE_LAUNCHTUTORIAL)

 protected:
  ~FirstRunPrivateLaunchTutorialFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_FIRST_RUN_PRIVATE_FIRST_RUN_PRIVATE_API_H_

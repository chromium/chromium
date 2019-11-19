// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_WEBSTORE_WIDGET_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_WEBSTORE_WIDGET_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/browser/extension_function.h"

namespace chrome_apps {
namespace api {

class WebstoreWidgetPrivateInstallWebstoreItemFunction
    : public ExtensionFunction {
 public:
  WebstoreWidgetPrivateInstallWebstoreItemFunction();

  DECLARE_EXTENSION_FUNCTION("webstoreWidgetPrivate.installWebstoreItem",
                             WEBSTOREWIDGETPRIVATE_INSTALLWEBSTOREITEM)

 protected:
  ~WebstoreWidgetPrivateInstallWebstoreItemFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnInstallComplete(bool success,
                         const std::string& error,
                         extensions::webstore_install::Result result);

  DISALLOW_COPY_AND_ASSIGN(WebstoreWidgetPrivateInstallWebstoreItemFunction);
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_WEBSTORE_WIDGET_PRIVATE_WEBSTORE_WIDGET_PRIVATE_API_H_

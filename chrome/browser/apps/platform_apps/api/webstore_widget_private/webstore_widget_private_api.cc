// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/webstore_widget_private/webstore_widget_private_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/platform_apps/api/webstore_widget_private/app_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/apps/platform_apps/api/webstore_widget_private.h"
#include "extensions/browser/extension_function_constants.h"

namespace chrome_apps {
namespace api {

WebstoreWidgetPrivateInstallWebstoreItemFunction::
    WebstoreWidgetPrivateInstallWebstoreItemFunction() {}

WebstoreWidgetPrivateInstallWebstoreItemFunction::
    ~WebstoreWidgetPrivateInstallWebstoreItemFunction() {}

ExtensionFunction::ResponseAction
WebstoreWidgetPrivateInstallWebstoreItemFunction::Run() {
  const std::unique_ptr<webstore_widget_private::InstallWebstoreItem::Params>
      params(
          webstore_widget_private::InstallWebstoreItem::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->item_id.empty())
    return RespondNow(Error("App ID empty."));

  if (params->silent_installation)
    return RespondNow(Error("Silent installation not allowed."));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(
        Error(extensions::function_constants::kCouldNotFindSenderWebContents));
  }

  auto installer = base::MakeRefCounted<webstore_widget::AppInstaller>(
      web_contents, params->item_id,
      Profile::FromBrowserContext(browser_context()),
      params->silent_installation,
      base::BindOnce(
          &WebstoreWidgetPrivateInstallWebstoreItemFunction::OnInstallComplete,
          this));
  // installer will be AddRef()'d in BeginInstall().
  installer->BeginInstall();

  return RespondLater();
}

void WebstoreWidgetPrivateInstallWebstoreItemFunction::OnInstallComplete(
    bool success,
    const std::string& error,
    extensions::webstore_install::Result result) {
  Respond(success ? NoArguments() : Error(error));
}

}  // namespace api
}  // namespace chrome_apps

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_file_manager_ui_delegate.h"

#include <memory.h>

#include "ash/webui/file_manager/url_constants.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"
#include "url/origin.h"

using ash::file_manager::kChromeUIFileManagerURL;

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  DCHECK(web_ui_);

  auto* browser_context = web_ui_->GetWebContents()->GetBrowserContext();
  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIFileManagerURL));
  allowlist->RegisterAutoGrantedPermissions(
      host_origin, {ContentSettingsType::FILE_HANDLING});
}

void ChromeFileManagerUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) const {
  base::Value dict = GetFileManagerStrings();

  const std::string locale = g_browser_process->GetApplicationLocale();
  AddFileManagerFeatureStrings(locale, Profile::FromWebUI(web_ui_), &dict);

  source->AddLocalizedStrings(base::Value::AsDictionaryValue(dict));
}

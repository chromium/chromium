// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_file_manager_ui_delegate.h"

#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  DCHECK(web_ui_);
}

base::Value::Dict ChromeFileManagerUIDelegate::GetLoadTimeData() const {
  base::Value::Dict dict = GetFileManagerStrings();

  const std::string locale = g_browser_process->GetApplicationLocale();
  AddFileManagerFeatureStrings(locale, Profile::FromWebUI(web_ui_), &dict);
  return dict;
}

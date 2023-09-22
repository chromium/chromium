// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"

#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace apps {

void CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack())
    web_contents->GetController().GoBack();
  else
    web_contents->ClosePage();
}

PickerEntryType GetPickerEntryType(AppType app_type) {
  PickerEntryType picker_entry_type = PickerEntryType::kUnknown;
  switch (app_type) {
    case AppType::kUnknown:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kBruschetta:
    case AppType::kStandaloneBrowserExtension:
      break;
    case AppType::kArc:
      picker_entry_type = PickerEntryType::kArc;
      break;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      picker_entry_type = PickerEntryType::kWeb;
      break;
    case AppType::kMacOs:
      picker_entry_type = PickerEntryType::kMacOs;
      break;
  }
  return picker_entry_type;
}

}  // namespace apps

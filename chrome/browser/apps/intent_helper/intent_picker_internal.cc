// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"

#include <utility>

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace apps {

void ShowIntentPickerBubbleForApps(content::WebContents* web_contents,
                                   std::vector<IntentPickerAppInfo> apps,
                                   bool show_stay_in_chrome,
                                   bool show_remember_selection,
                                   IntentPickerResponse callback) {
  if (apps.empty())
    return;

  // It should be safe to bind |web_contents| since closing the current tab will
  // close the intent picker and run the callback prior to the WebContents being
  // deallocated.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  browser->window()->ShowIntentPickerBubble(
      std::move(apps), show_stay_in_chrome, show_remember_selection,
      IntentPickerBubbleType::kLinkCapturing, absl::nullopt,
      std::move(callback));
}

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

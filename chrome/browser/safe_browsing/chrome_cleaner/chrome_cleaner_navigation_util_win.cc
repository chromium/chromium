// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace chrome_cleaner_util {

namespace {
GURL GetCleanupPageURL() {
  return chrome::GetSettingsUrl(chrome::kCleanupSubPage);
}
}  // namespace

Browser* FindBrowser() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator browser_iterator =
           browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    if (browser->is_type_normal() &&
        (browser->window()->IsActive() || !browser->window()->IsMinimized())) {
      return browser;
    }
  }

  return nullptr;
}

bool CleanupPageIsActiveTab(Browser* browser) {
  DCHECK(browser);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return web_contents &&
         web_contents->GetLastCommittedURL() == GetCleanupPageURL();
}

void OpenCleanupPage(Browser* browser, WindowOpenDisposition disposition) {
  DCHECK(browser);

  browser->OpenURL(content::OpenURLParams(
      GetCleanupPageURL(), content::Referrer(), disposition,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
}

}  // namespace chrome_cleaner_util

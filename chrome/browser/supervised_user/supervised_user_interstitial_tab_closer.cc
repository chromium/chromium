// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial_tab_closer.h"

#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

TabCloser::~TabCloser() = default;

// static
void TabCloser::CheckIfInBrowserThenCloseTab(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // Close the tab only if there is a browser for it (which is not the case
  // for example in a <webview>).
#if !BUILDFLAG(IS_ANDROID)
  if (!chrome::FindBrowserWithTab(web_contents)) {
    return;
  }
#endif
  TabCloser::CreateForWebContents(web_contents);
}

TabCloser::TabCloser(content::WebContents* web_contents)
    : content::WebContentsUserData<TabCloser>(*web_contents) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TabCloser::CloseTabImpl, weak_ptr_factory_.GetWeakPtr()));
}

void TabCloser::CloseTabImpl() {
  // On Android, FindBrowserWithTab and TabStripModel don't exist.
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  DCHECK(browser);
  TabStripModel* tab_strip = browser->tab_strip_model();
  DCHECK_NE(TabStripModel::kNoTab,
            tab_strip->GetIndexOfWebContents(&GetWebContents()));
  if (tab_strip->count() <= 1) {
    // Don't close the last tab in the window.
    GetWebContents().RemoveUserData(UserDataKey());
    return;
  }
#endif
  GetWebContents().Close();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCloser);

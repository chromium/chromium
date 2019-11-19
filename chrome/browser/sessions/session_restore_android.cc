// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <vector>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

// The android implementation does not do anything "foreign session" specific.
// We use it to restore tabs from "recently closed" too.
// static
content::WebContents* SessionRestore::RestoreForeignSessionTab(
    content::WebContents* web_contents,
    const sessions::SessionTab& session_tab,
    WindowOpenDisposition disposition) {
  DCHECK(session_tab.navigations.size() > 0);
  content::BrowserContext* context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  DCHECK(tab_model);
  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          session_tab.navigations, profile);
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(context));
  content::WebContents* raw_new_web_contents = new_web_contents.get();
  int selected_index = session_tab.normalized_navigation_index();
  new_web_contents->GetController().Restore(
      selected_index, content::RestoreType::LAST_SESSION_EXITED_CLEANLY,
      &entries);

  TabAndroid* current_tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(current_tab);
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    web_contents->GetDelegate()->SwapWebContents(
        web_contents, std::move(new_web_contents), false, false);
  } else {
    DCHECK(disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
           disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB);
    tab_model->CreateTab(current_tab, new_web_contents.release());
  }
  return raw_new_web_contents;
}

// static
std::vector<Browser*> SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end) {
  NOTREACHED();
  return std::vector<Browser*>();
}

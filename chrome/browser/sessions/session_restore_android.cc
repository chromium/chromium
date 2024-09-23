// Copyright 2012 The Chromium Authors
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
#include "ui/base/window_open_disposition.h"

// The android implementation does not do anything "foreign session" specific.
// We use it to restore tabs from "recently closed" too.
// static
content::WebContents* SessionRestore::RestoreForeignSessionTab(
    content::WebContents* web_contents,
    const sessions::SessionTab& session_tab,
    WindowOpenDisposition disposition,
    bool skip_renderer_creation) {
  DCHECK(session_tab.navigations.size() > 0);
  content::BrowserContext* context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  DCHECK(tab_model);
  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          session_tab.navigations, profile);

  bool is_background_tab =
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  content::WebContents::CreateParams create_params(context);
  if (is_background_tab && skip_renderer_creation) {
    create_params.initially_hidden = true;
    create_params.desired_renderer_state =
        content::WebContents::CreateParams::kNoRendererProcess;
  }
  // Ensure that skipping renderer creation is only enabled for background tabs.
  DCHECK(skip_renderer_creation ? is_background_tab : true);
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(create_params);

  content::WebContents* raw_new_web_contents = new_web_contents.get();
  int selected_index = session_tab.normalized_navigation_index();
  new_web_contents->GetController().Restore(
      selected_index, content::RestoreType::kRestored, &entries);

  TabAndroid* current_tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(current_tab);
  // If swapped, return the current tab's most up-to-date web contents.
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    current_tab->SwapWebContents(std::move(new_web_contents), false, false);
    return current_tab->web_contents();
  }
  DCHECK(disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
         disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB);
  tab_model->CreateTab(
      current_tab, new_web_contents.release(),
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB);
  return raw_new_web_contents;
}

// static
std::vector<Browser*> SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end) {
  NOTREACHED_IN_MIGRATION();
  return std::vector<Browser*>();
}

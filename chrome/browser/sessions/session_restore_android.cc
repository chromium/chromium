// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <vector>

#include "base/barrier_callback.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/window_open_disposition.h"

namespace {

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
BrowserWindowInterface::Type BrowserTypeFromWindowType(
    sessions::SessionWindow::WindowType type) {
  switch (type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      return BrowserWindowInterface::TYPE_NORMAL;
    case sessions::SessionWindow::TYPE_POPUP:
      return BrowserWindowInterface::TYPE_POPUP;
    case sessions::SessionWindow::TYPE_APP:
      return BrowserWindowInterface::TYPE_APP;
    case sessions::SessionWindow::TYPE_DEVTOOLS:
      // BrowserWindowInterface does not have a window type for devtools.
      return BrowserWindowInterface::TYPE_NORMAL;
    case sessions::SessionWindow::TYPE_APP_POPUP:
      return BrowserWindowInterface::TYPE_APP_POPUP;
  }
}
#endif  // #if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

}  // namespace

// The android implementation does not do anything "foreign session" specific.
// It is also used to replace the current tab when restoring from "recently
// closed".
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
  // Fake replacing the current tab's WebContents with a new one by creating and
  // selecting a new tab then closing the old one. Using the current tab as the
  // parent will ensure group state, position, etc. should be kept.
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    // This will never be a bulk session restore so we can select the tab here.
    tab_model->CreateTab(current_tab, std::move(new_web_contents),
                         TabModel::kInvalidIndex,
                         TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND,
                         /*should_pin=*/false);
    tab_model->CloseTab(current_tab->GetHandle());
    return raw_new_web_contents;
  }
  DCHECK(disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
         disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB);
  // Do not select a tab here it will interrupt bulk session restores.
  tab_model->CreateTab(
      current_tab, std::move(new_web_contents), TabModel::kInvalidIndex,
      TabModel::TabLaunchType::FROM_RECENT_TABS, /*should_pin=*/false);
  return raw_new_web_contents;
}

// static
void SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end,
    base::OnceCallback<void(std::vector<BrowserWindowInterface*>)> callback) {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // The extensions sessions API can restore foreign windows.
  size_t window_count = std::distance(begin, end);
  // Wait for `window_count` callbacks.
  auto barrier_callback = base::BarrierCallback<BrowserWindowInterface*>(
      window_count, std::move(callback));
  std::vector<BrowserWindowInterface*> windows;
  windows.reserve(window_count);
  for (auto it = begin; it != end; ++it) {
    BrowserWindowCreateParams params(*profile,
                                     /*from_user_gesture=*/false);
    params.type = BrowserTypeFromWindowType((*it)->type);
    params.initial_bounds = (*it)->bounds;
    params.app_name = (*it)->app_name;
    params.initial_show_state = (*it)->show_state;
    // When `window_count` windows have been created, `barrier_callback` will
    // have accumulated the created BrowserWindowInterfaces and will invoke
    // `callback` with them.
    CreateBrowserWindow(std::move(params), barrier_callback);
  }
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
}

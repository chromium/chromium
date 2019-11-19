// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using base::TimeTicks;
using content::WebContents;

namespace resource_coordinator {

TabManager::WebContentsData::WebContentsData(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TabManager::WebContentsData::~WebContentsData() {}

void TabManager::WebContentsData::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only change to the loading state if there is a navigation in the main
  // frame. DidStartLoading() happens before this, but at that point we don't
  // know if the load is happening in the main frame or an iframe.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  g_browser_process->GetTabManager()
      ->stats_collector()
      ->OnDidStartMainFrameNavigation(web_contents());
}

void TabManager::WebContentsData::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  SetIsInSessionRestore(false);
  g_browser_process->GetTabManager()->OnDidFinishNavigation(navigation_handle);
}

void TabManager::WebContentsData::WebContentsDestroyed() {
  // If Chrome is shutting down, ignore this event.
  if (g_browser_process->IsShuttingDown())
    return;
  SetIsInSessionRestore(false);
  g_browser_process->GetTabManager()->OnWebContentsDestroyed(web_contents());
}

// static
void TabManager::WebContentsData::CopyState(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Only copy if an existing state is found.
  if (FromWebContents(old_contents)) {
    CreateForWebContents(new_contents);
    FromWebContents(new_contents)->tab_data_ =
        FromWebContents(old_contents)->tab_data_;
  }
}

TabManager::WebContentsData::Data::Data()
    : tab_loading_state(TabLoadTracker::LoadingState::UNLOADED),
      is_in_session_restore(false),
      is_restored_in_foreground(false) {}

bool TabManager::WebContentsData::Data::operator==(const Data& right) const {
  return tab_loading_state == right.tab_loading_state &&
         is_in_session_restore == right.is_in_session_restore &&
         is_restored_in_foreground == right.is_restored_in_foreground;
}

bool TabManager::WebContentsData::Data::operator!=(const Data& right) const {
  return !(*this == right);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabManager::WebContentsData)

}  // namespace resource_coordinator

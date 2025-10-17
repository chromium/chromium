// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_source_observer.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace glic {

GlicTabSourceObserver::GlicTabSourceObserver(GlicWindowController* coordinator,
                                             Profile* profile)
    : coordinator_(coordinator), profile_(profile) {
  BrowserList::GetInstance()->AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }
}

GlicTabSourceObserver::~GlicTabSourceObserver() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserRemoved(browser);
  }
  BrowserList::GetInstance()->RemoveObserver(this);
}

void GlicTabSourceObserver::MaybeAddSidePanel(
    tabs::TabInterface* tab,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  content::RenderFrameHost* opener_rfh = web_contents->GetOpener();
  if (!opener_rfh) {
    return;
  }

  content::WebContents* opener_contents =
      content::WebContents::FromRenderFrameHost(opener_rfh);

  // TODO(crbug.com/447208578): For normal link clicks inside a tab, check for a
  // source instance inside opener_rfh.
  if (opener_contents &&
      coordinator_->host_manager().IsGlicWebUiHost(opener_rfh->GetProcess())) {
    coordinator_->FindInstanceFromGlicContentsAndBindToTab(
        opener_contents->GetOutermostWebContents(), tab);
  }
}

void GlicTabSourceObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }

  for (const auto& change_insert : change.GetInsert()->contents) {
    MaybeAddSidePanel(change_insert.tab, change_insert.contents);
  }
}

void GlicTabSourceObserver::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_) {
    return;
  }

  browser->tab_strip_model()->AddObserver(this);
}

void GlicTabSourceObserver::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_) {
    return;
  }
  browser->tab_strip_model()->RemoveObserver(this);
}

}  // namespace glic

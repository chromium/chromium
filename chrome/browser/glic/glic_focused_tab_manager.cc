// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_focused_tab_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace glic {

constexpr int kMaxActiveWebContents = 50;

GlicFocusedTabManager::GlicFocusedTabManager(Profile* profile)
    : profile_(profile), activated_web_contents_() {
  BrowserList::GetInstance()->AddObserver(this);
  // Observe existing windows
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }
}

GlicFocusedTabManager::~GlicFocusedTabManager() {
  BrowserList::GetInstance()->RemoveObserver(this);
  // Stop observing existing windows
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserRemoved(browser);
  }
}

void GlicFocusedTabManager::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->AddObserver(this);
    HandleWebContentsActivated(
        browser->tab_strip_model()->GetActiveWebContents());
  }
}

void GlicFocusedTabManager::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

bool GlicFocusedTabManager::IsValidFocusable(
    content::WebContents* web_contents) {
  return web_contents->GetURL().SchemeIsHTTPOrHTTPS() ||
         web_contents->GetURL().SchemeIsFile();
}

void GlicFocusedTabManager::HandleWebContentsActivated(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto it = FindActivatedWebContents(web_contents);
  if (it == activated_web_contents_.end()) {
    // Not found, add to front
    activated_web_contents_.insert(activated_web_contents_.begin(),
                                   web_contents->GetWeakPtr());
    if (activated_web_contents_.size() > kMaxActiveWebContents) {
      activated_web_contents_.pop_back();
    }
  } else {
    // Found, move to front
    if (it != activated_web_contents_.begin()) {
      auto value = *it;
      activated_web_contents_.erase(it);
      activated_web_contents_.insert(activated_web_contents_.begin(), value);
    }
  }
}

void GlicFocusedTabManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    HandleWebContentsActivated(selection.new_contents);
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& removed_tab : change.GetRemove()->contents) {
      content::WebContents* removed_contents = removed_tab.contents;

      auto it = std::remove_if(
          activated_web_contents_.begin(), activated_web_contents_.end(),
          [removed_contents](
              const base::WeakPtr<content::WebContents>& weak_ptr) {
            return weak_ptr.get() == removed_contents;
          });
      activated_web_contents_.erase(it, activated_web_contents_.end());
    }
  }

  if (change.type() == TabStripModelChange::kReplaced) {
    auto it = FindActivatedWebContents(change.GetReplace()->old_contents);
    if (it != activated_web_contents_.end()) {
      *it = change.GetReplace()->new_contents->GetWeakPtr();
    }
  }
}

std::vector<base::WeakPtr<content::WebContents>>::iterator
GlicFocusedTabManager::FindActivatedWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return activated_web_contents_.end();
  }

  return base::ranges::find_if(
      activated_web_contents_,
      [web_contents](const base::WeakPtr<content::WebContents>& weak_ptr) {
        return weak_ptr.get() == web_contents;
      });
}

content::WebContents* GlicFocusedTabManager::GetWebContentsForFocusedTab() {
  for (base::WeakPtr<content::WebContents> web_contents :
       activated_web_contents_) {
    if (web_contents && IsValidFocusable(web_contents.get())) {
      return web_contents.get();
    }
  }
  return nullptr;
}

}  // namespace glic

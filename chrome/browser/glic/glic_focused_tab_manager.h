// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {
// Responsible for managing which tab is considered "focused" and for accessing
// its WebContents.
class GlicFocusedTabManager : public BrowserListObserver,
                              public TabStripModelObserver {
 public:
  explicit GlicFocusedTabManager(Profile* profile);
  ~GlicFocusedTabManager() override;

  GlicFocusedTabManager(const GlicFocusedTabManager&) = delete;
  GlicFocusedTabManager& operator=(const GlicFocusedTabManager&) = delete;

  content::WebContents* GetWebContentsForFocusedTab();

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  bool IsValidFocusable(content::WebContents* web_contents);
  void HandleWebContentsActivated(content::WebContents* web_contents);
  std::vector<base::WeakPtr<content::WebContents>>::iterator
  FindActivatedWebContents(content::WebContents* web_contents);

  raw_ptr<Profile> profile_;
  base::WeakPtr<content::WebContents> focused_web_contents_;
  std::vector<base::WeakPtr<content::WebContents>> activated_web_contents_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_

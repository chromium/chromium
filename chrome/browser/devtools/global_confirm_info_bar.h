// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_
#define CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

// GlobalConfirmInfoBar is shown for every tab in every browser until it
// is dismissed or the close method is called.
// It listens to all tabs in all browsers and adds/removes confirm infobar
// to each of the tabs.
// TODO(pkasting): This is a hack, driven by the original design of infobars
// being tab-scoped.  Either this should be replaced by a different UI for
// whole-browser notifications, or the core infobar APIs should better
// accommodate these sorts of infobars (e.g. with a separate "global infobar
// manager" object or the like).
class GlobalConfirmInfoBar : public TabStripModelObserver,
                             public infobars::InfoBarManager::Observer {
 public:
  // Attempts to show a global infobar for |delegate|.  If infobar addition
  // fails (e.g. because infobars are disabled), the global infobar will not
  // appear, and it (and |delegate|) will be deleted asynchronously.  Otherwise,
  // the delegate will be deleted synchronously when any of the tabs' infobars
  // is closed via user action.  Note that both of these aspects of lifetime
  // management differ from how typical infobars work.
  static GlobalConfirmInfoBar* Show(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  GlobalConfirmInfoBar(const GlobalConfirmInfoBar&) = delete;
  GlobalConfirmInfoBar& operator=(const GlobalConfirmInfoBar&) = delete;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* info_bar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  // Closes all the infobars.
  void Close();

 private:
  class DelegateProxy;

  explicit GlobalConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~GlobalConfirmInfoBar() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* web_contents,
                    int index,
                    TabChangeType change_type) override;

  // Adds the info bar to the tab if it is missing.
  void MaybeAddInfoBar(content::WebContents* web_contents);

  std::unique_ptr<ConfirmInfoBarDelegate> delegate_;
  std::map<infobars::InfoBarManager*, raw_ptr<DelegateProxy, CtnExperimental>>
      proxies_;
  BrowserTabStripTracker browser_tab_strip_tracker_{this, nullptr};

  // Indicates if the global infobar is currently in the process of shutting
  // down.
  bool is_closing_ = false;

  base::WeakPtrFactory<GlobalConfirmInfoBar> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_GLOBAL_CONFIRM_INFO_BAR_H_

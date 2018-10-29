// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace content {
struct LoadCommittedDetails;
class WebContents;
}

namespace infobars {
class InfoBar;
}

// Associates a Tab to a InfoBarManager and manages its lifetime.
// It manages the infobar notifications and responds to navigation events.
class InfoBarService : public infobars::InfoBarManager,
                       public content::WebContentsObserver,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  ~InfoBarService() override;

  static infobars::InfoBarDelegate::NavigationDetails
      NavigationDetailsFromLoadCommittedDetails(
          const content::LoadCommittedDetails& details);

  // This function must only be called on infobars that are owned by an
  // InfoBarService instance (or not owned at all, in which case this returns
  // NULL).
  static content::WebContents* WebContentsFromInfoBar(
      infobars::InfoBar* infobar);

  // Makes it so the next reload is ignored. That is, if the next commit is a
  // reload then it is treated as if nothing happened and no infobars are
  // attempted to be closed.
  // This is useful for non-user triggered reloads that should not dismiss
  // infobars. For example, instant may trigger a reload when the google URL
  // changes.
  void set_ignore_next_reload() { ignore_next_reload_ = true; }

  // InfoBarManager:
  // TODO(sdefresne): Change clients to invoke this on infobars::InfoBarManager
  // and turn the method override private.
  std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate) override;
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override;

 protected:
  explicit InfoBarService(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  // InfoBarManager:
  int GetActiveEntryID() override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

  // See description in set_ignore_next_reload().
  bool ignore_next_reload_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

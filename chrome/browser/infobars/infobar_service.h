// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

// //chrome's specialization of ContentInfoBarManager, which implements creation
// of confirm infobars and ties the lifetime of ContentInfoBarManager instances
// to that of the WebContents with which they are associated.
class InfoBarService : public infobars::ContentInfoBarManager,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  ~InfoBarService() override;

  // Cross-platform method for creating a confirm infobar.
  // TODO(crbug.com/1199686): Move this to a static helper function as part of
  // eliminating the //chrome-level InfoBarService.
  virtual std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

 protected:
  explicit InfoBarService(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  // infobars::ContentInfoBarManager:
  void WebContentsDestroyed() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(InfoBarService);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SERVICE_H_

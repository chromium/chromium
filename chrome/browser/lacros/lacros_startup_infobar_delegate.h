// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_STARTUP_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_LACROS_LACROS_STARTUP_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "url/gurl.h"

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

// This class is responsible for configuring the info bar that is shown the
// first time the user shows Lacros. This info bar provides more information
// about Lacros as well as a link to get more information.
class LacrosStartupInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  LacrosStartupInfoBarDelegate();
  ~LacrosStartupInfoBarDelegate() override;
  LacrosStartupInfoBarDelegate(const LacrosStartupInfoBarDelegate&) = delete;
  LacrosStartupInfoBarDelegate& operator=(const LacrosStartupInfoBarDelegate&) =
      delete;

  // Creates and shows the infobar.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
};

#endif  // CHROME_BROWSER_LACROS_LACROS_STARTUP_INFOBAR_DELEGATE_H_

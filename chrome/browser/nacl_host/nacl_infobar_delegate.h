// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

class NaClInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a NaCl infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

  NaClInfoBarDelegate(const NaClInfoBarDelegate&) = delete;
  NaClInfoBarDelegate& operator=(const NaClInfoBarDelegate&) = delete;

 private:
  NaClInfoBarDelegate();
  ~NaClInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_

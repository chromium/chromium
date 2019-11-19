// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INFOBAR_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

// Base class for some of the password manager infobar delegates, e.g.
// SavePasswordInfoBarDelegate.
class PasswordManagerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ~PasswordManagerInfoBarDelegate() override;

  // Getter for the message displayed in adition to the title. If no message
  // was set, this returns and empty string.
  base::string16 GetDetailsMessageText() const;

  // ConfirmInfoBarDelegate:
  InfoBarAutomationType GetInfoBarAutomationType() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

 protected:
  PasswordManagerInfoBarDelegate();

  void SetMessage(const base::string16& message);
  void SetDetailsMessage(const base::string16& details_message);

 private:
  // Message for the infobar: branded as a part of Google Smart Lock for signed
  // users.
  base::string16 message_;

  // Used to display aditional information about where the passwords were saved.
  base::string16 details_message_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INFOBAR_DELEGATE_ANDROID_H_

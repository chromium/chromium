// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

namespace extensions {

class IncognitoConnectabilityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using InfoBarCallback = base::OnceCallback<void(
      IncognitoConnectability::ScopedAlertTracker::Mode)>;

  // Creates a confirmation infobar and delegate and adds the infobar to
  // |infobar_service|.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   const base::string16& message,
                                   InfoBarCallback callback);

  // Marks the infobar as answered so that the callback is not executed when the
  // delegate is destroyed.
  void set_answered() { answered_ = true; }

 private:
  IncognitoConnectabilityInfoBarDelegate(const base::string16& message,
                                         InfoBarCallback callback);
  ~IncognitoConnectabilityInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  base::string16 message_;
  bool answered_;
  InfoBarCallback callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_

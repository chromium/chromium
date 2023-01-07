// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

namespace extensions {

class IncognitoConnectabilityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using InfoBarCallback = base::OnceCallback<void(
      IncognitoConnectability::ScopedAlertTracker::Mode)>;

  // Creates a confirmation infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager,
      const std::u16string& message,
      InfoBarCallback callback);

  // Marks the infobar as answered so that the callback is not executed when the
  // delegate is destroyed.
  void set_answered() { answered_ = true; }

 private:
  IncognitoConnectabilityInfoBarDelegate(const std::u16string& message,
                                         InfoBarCallback callback);
  ~IncognitoConnectabilityInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  std::u16string message_;
  bool answered_;
  InfoBarCallback callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE_H_

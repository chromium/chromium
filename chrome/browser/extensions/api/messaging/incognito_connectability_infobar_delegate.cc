// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"

#include <utility>

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

// static
infobars::InfoBar* IncognitoConnectabilityInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const std::u16string& message,
    IncognitoConnectabilityInfoBarDelegate::InfoBarCallback callback) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new IncognitoConnectabilityInfoBarDelegate(message,
                                                     std::move(callback)))));
}

IncognitoConnectabilityInfoBarDelegate::IncognitoConnectabilityInfoBarDelegate(
    const std::u16string& message,
    InfoBarCallback callback)
    : message_(message), answered_(false), callback_(std::move(callback)) {}

IncognitoConnectabilityInfoBarDelegate::
    ~IncognitoConnectabilityInfoBarDelegate() {
  if (!answered_) {
    // The infobar has closed without the user expressing an explicit
    // preference. The current request should be denied but further requests
    // should show an interactive prompt.
    std::move(callback_).Run(
        IncognitoConnectability::ScopedAlertTracker::INTERACTIVE);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
IncognitoConnectabilityInfoBarDelegate::GetIdentifier() const {
  return INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE;
}

std::u16string IncognitoConnectabilityInfoBarDelegate::GetMessageText() const {
  return message_;
}

std::u16string IncognitoConnectabilityInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_PERMISSION_ALLOW
                                                         : IDS_PERMISSION_DENY);
}

bool IncognitoConnectabilityInfoBarDelegate::Accept() {
  std::move(callback_).Run(
      IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);
  answered_ = true;
  return true;
}

bool IncognitoConnectabilityInfoBarDelegate::Cancel() {
  std::move(callback_).Run(
      IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);
  answered_ = true;
  return true;
}

}  // namespace extensions

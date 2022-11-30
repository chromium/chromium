// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_infobar_delegate.h"

#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
void DevToolsInfoBarDelegate::Create(const std::u16string& message,
                                     Callback callback) {
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new DevToolsInfoBarDelegate(message, std::move(callback)));
  GlobalConfirmInfoBar::Show(std::move(delegate));
}

DevToolsInfoBarDelegate::DevToolsInfoBarDelegate(const std::u16string& message,
                                                 Callback callback)
    : message_(message), callback_(std::move(callback)) {}

DevToolsInfoBarDelegate::~DevToolsInfoBarDelegate() {
  if (!callback_.is_null())
    std::move(callback_).Run(false);
}

infobars::InfoBarDelegate::InfoBarIdentifier
DevToolsInfoBarDelegate::GetIdentifier() const {
  return DEV_TOOLS_INFOBAR_DELEGATE;
}

std::u16string DevToolsInfoBarDelegate::GetMessageText() const {
  return message_;
}

std::u16string DevToolsInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_DEV_TOOLS_CONFIRM_ALLOW_BUTTON
                                       : IDS_DEV_TOOLS_CONFIRM_DENY_BUTTON);
}

bool DevToolsInfoBarDelegate::Accept() {
  std::move(callback_).Run(true);
  return true;
}

bool DevToolsInfoBarDelegate::Cancel() {
  std::move(callback_).Run(false);
  return true;
}

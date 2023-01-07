// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class DevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  static void Create(const std::u16string& message, Callback callback);

  DevToolsInfoBarDelegate(const DevToolsInfoBarDelegate&) = delete;
  DevToolsInfoBarDelegate& operator=(const DevToolsInfoBarDelegate&) = delete;

 private:
  DevToolsInfoBarDelegate(const std::u16string& message, Callback callback);
  ~DevToolsInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  const std::u16string message_;
  Callback callback_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

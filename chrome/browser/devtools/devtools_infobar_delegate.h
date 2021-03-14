// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class DevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  static void Create(const std::u16string& message, Callback callback);

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

  DISALLOW_COPY_AND_ASSIGN(DevToolsInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

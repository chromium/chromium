// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class DevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  static void Create(const base::string16& message, Callback callback);

 private:
  DevToolsInfoBarDelegate(const base::string16& message, Callback callback);
  ~DevToolsInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  const base::string16 message_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_INFOBAR_DELEGATE_H_

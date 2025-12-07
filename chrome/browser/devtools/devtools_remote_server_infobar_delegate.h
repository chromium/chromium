// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_REMOTE_SERVER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_REMOTE_SERVER_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class Browser;

// An infobar used to globally warn users a CDP connection
// was established.
class DevToolsRemoteServerInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit DevToolsRemoteServerInfobarDelegate(Browser* browser);
  DevToolsRemoteServerInfobarDelegate(
      const DevToolsRemoteServerInfobarDelegate&) = delete;
  DevToolsRemoteServerInfobarDelegate& operator=(
      const DevToolsRemoteServerInfobarDelegate&) = delete;
  ~DevToolsRemoteServerInfobarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

 private:
  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_REMOTE_SERVER_INFOBAR_DELEGATE_H_

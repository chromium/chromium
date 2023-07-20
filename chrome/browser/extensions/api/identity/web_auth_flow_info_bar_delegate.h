// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_INFO_BAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_INFO_BAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Infobar used by extension auth flow `chrome.identity.launchWebAuthFlow()`
// when authentication is done through a Browser Tab. A browser tab is opened
// when needing action from the user in this flow.
// This infobar displays information to the user to clarify why this tab was
// opened, mentioning the extension name as part of the text. Auth flows should
// take care of managing when to close the bar if not manually closed by the
// user, otherwise it should live as long as the flow is alive.
class WebAuthFlowInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static base::WeakPtr<WebAuthFlowInfoBarDelegate> Create(
      content::WebContents* web_contents,
      const std::string& extension_name);

  ~WebAuthFlowInfoBarDelegate() override;

  // infobars::InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;

  // ConfirmInfoBarDelegate:
  std::u16string GetMessageText() const override;
  int GetButtons() const override;

  // Closes the info bar this delegate is associated with.
  void CloseInfoBar();

 private:
  explicit WebAuthFlowInfoBarDelegate(const std::string& extension_name);

  const std::string extension_name_;

  base::WeakPtrFactory<WebAuthFlowInfoBarDelegate> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_INFO_BAR_DELEGATE_H_

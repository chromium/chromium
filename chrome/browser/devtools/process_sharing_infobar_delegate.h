// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROCESS_SHARING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_PROCESS_SHARING_INFOBAR_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

class ProcessSharingInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit ProcessSharingInfobarDelegate(content::WebContents* web_contents);
  ~ProcessSharingInfobarDelegate() override;

  // ConfirmInfoBarDelegate:
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool Accept() override;

  // infobars::InfoBarDelegate
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

 private:
  base::WeakPtr<content::WebContents> inspected_web_contents_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROCESS_SHARING_INFOBAR_DELEGATE_H_

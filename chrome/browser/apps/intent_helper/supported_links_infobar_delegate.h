// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

class Profile;

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

namespace apps {

// An infobar delegate asking if the user wants to enable the Supported Links
// setting for an app.
class SupportedLinksInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit SupportedLinksInfoBarDelegate(Profile* profile,
                                         const std::string& app_id);

  SupportedLinksInfoBarDelegate(const SupportedLinksInfoBarDelegate&) = delete;
  SupportedLinksInfoBarDelegate& operator=(
      const SupportedLinksInfoBarDelegate&) = delete;

  // Creates and shows a supported links infobar for the given |web_contents|.
  // The infobar will only be created if it is suitable for the given |app_id|
  // (e.g. the app does not already have the supported links setting enabled).
  static void MaybeShowSupportedLinksInfoBar(content::WebContents* web_contents,
                                             const std::string& app_id);

 private:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  bool Accept() override;
  bool Cancel() override;

  Profile* profile_;
  std::string app_id_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_DELEGATE_H_

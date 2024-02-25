// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace infobars {
class InfoBar;
}

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

namespace web_app {
class WebAppProvider;
}

namespace apps {

// An infobar delegate asking if the user wants to enable link capturing for
// the given application. This is only created when the app doesn't have link
// capturing already enabled.
// Note: This InfoBar is only used on non-CrOS platforms.
class EnableLinkCapturingInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  EnableLinkCapturingInfoBarDelegate(
      const EnableLinkCapturingInfoBarDelegate&) = delete;
  EnableLinkCapturingInfoBarDelegate& operator=(
      const EnableLinkCapturingInfoBarDelegate&) = delete;
  ~EnableLinkCapturingInfoBarDelegate() override;

  // Searches the toolbar on the web contents for this infobar, and returns it
  // if found.
  static infobars::InfoBar* FindInfoBar(content::WebContents* web_contents);

  // Creates and shows a supported links infobar for the given |web_contents|.
  // The infobar will only be created if it is suitable for the given |app_id|
  // (e.g. the app does not already have the supported links setting enabled).
  // This will CHECK-fail if this infobar is already added to the web contents,
  // so ensure that doesn't happen.
  static std::unique_ptr<EnableLinkCapturingInfoBarDelegate> MaybeCreate(
      content::WebContents* web_contents,
      const webapps::AppId& app_id);

  // Removes the supported links infobar (if there is one) from the given
  // |web_contents|.
  static void RemoveInfoBar(content::WebContents* web_contents);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool IsCloseable() const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  EnableLinkCapturingInfoBarDelegate(Profile& profile,
                                     const webapps::AppId& app_id);

  raw_ref<Profile> profile_;
  raw_ref<web_app::WebAppProvider> provider_;
  webapps::AppId app_id_;
  bool action_taken_ = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE_H_

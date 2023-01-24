// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"

namespace extensions {

base::WeakPtr<WebAuthFlowInfoBarDelegate> WebAuthFlowInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::string& extension_name) {
  std::unique_ptr<WebAuthFlowInfoBarDelegate> delegate =
      base::WrapUnique(new WebAuthFlowInfoBarDelegate(extension_name));
  base::WeakPtr<WebAuthFlowInfoBarDelegate> weak_ptr =
      delegate->weak_factory_.GetWeakPtr();

  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  return weak_ptr;
}

WebAuthFlowInfoBarDelegate::WebAuthFlowInfoBarDelegate(
    const std::string& extension_name)
    : extension_name_(extension_name) {}

WebAuthFlowInfoBarDelegate::~WebAuthFlowInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
WebAuthFlowInfoBarDelegate::GetIdentifier() const {
  return InfoBarIdentifier::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE;
}

std::u16string WebAuthFlowInfoBarDelegate::GetMessageText() const {
  // TODO(https://crbug.com/1408402): The below hardcoded string is temporary.
  // Once the string to display is ready, replace the hardcoded string with a
  // translation string.
  return base::UTF8ToUTF16("Tab opened from extension -- " + extension_name_ +
                           " -- for authentication");
}

bool WebAuthFlowInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Infobar should not be closed as long as the auth flow is active.
  // Flows themselves should take care of closing the infobar when needed using
  // `WebAuthFlowInfoBarDelegate::CloseInfoBar()` and keeping a WeakPtr on
  // creation using static `WebAuthFlowInfoBarDelegate::Create()`.
  return false;
}

int WebAuthFlowInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

void WebAuthFlowInfoBarDelegate::CloseInfoBar() {
  infobar()->RemoveSelf();
}

}  // namespace extensions

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

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
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_LAUNCH_WEB_AUTH_FLOW_TAB_INFO_BAR_TEXT,
      base::UTF8ToUTF16(extension_name_));
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

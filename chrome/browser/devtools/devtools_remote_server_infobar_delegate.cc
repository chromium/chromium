// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_remote_server_infobar_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"

DevToolsRemoteServerInfobarDelegate::DevToolsRemoteServerInfobarDelegate(
    Browser* browser)
    : browser_(browser) {}

DevToolsRemoteServerInfobarDelegate::~DevToolsRemoteServerInfobarDelegate() =
    default;

infobars::InfoBarDelegate::InfoBarIdentifier
DevToolsRemoteServerInfobarDelegate::GetIdentifier() const {
  return DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE;
}

bool DevToolsRemoteServerInfobarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

std::u16string DevToolsRemoteServerInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_CONTROLLED_BY_AUTOMATION);
}

int DevToolsRemoteServerInfobarDelegate::GetButtons() const {
  // Android does not allow infobars with a solitary "Cancel" button, because
  // "Cancel" is considered a "secondary" button and cannot exist without a
  // primary button. Since the primary action here is to cancel, use BUTTON_OK
  // but label it as "Cancel" below and map Accept() to Cancel() below. This
  // works across platforms and avoids assertion failures deep in the Android
  // infobar code.
  return BUTTON_OK;
}

std::u16string DevToolsRemoteServerInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_CONNECTION_DIALOG_DISABLE_TEXT);
}

bool DevToolsRemoteServerInfobarDelegate::Accept() {
  // See comment in GetButtons() above.
  CHECK(browser_);
  GURL internal_url("chrome://inspect#remote-debugging");
  NavigateParams params(browser_, internal_url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  return ConfirmInfoBarDelegate::Accept();
}

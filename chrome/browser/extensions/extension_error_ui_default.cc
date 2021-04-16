// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

std::u16string GenerateTitle(const ExtensionSet& forbidden) {
  int app_count = 0;
  int extension_count = 0;
  for (const auto& extension : forbidden) {
    if (extension->is_app())
      app_count++;
    else
      extension_count++;
  }

  if ((app_count > 0) && (extension_count > 0)) {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_AND_APP_ALERT_TITLE);
  }
  if (app_count > 0) {
    return l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, app_count);
  }
  return l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE,
                                          extension_count);
}

std::vector<std::u16string> GenerateMessage(
    const ExtensionSet& forbidden,
    content::BrowserContext* browser_context) {
  std::vector<std::u16string> message;
  message.reserve(forbidden.size());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  for (const auto& extension : forbidden) {
    BlocklistState blocklist_state =
        prefs->GetExtensionBlocklistState(extension->id());
    bool disable_remotely_for_malware = prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_REMOTELY_FOR_MALWARE);
    int id = 0;
    if (disable_remotely_for_malware ||
        (blocklist_state == BlocklistState::BLOCKLISTED_MALWARE)) {
      id = IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_MALWARE;
    } else {
      id = extension->is_app() ? IDS_APP_ALERT_ITEM_BLOCKLISTED_OTHER
                               : IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_OTHER;
    }
    message.push_back(
        l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension->name())));
  }
  return message;
}

}  // namespace

class ExtensionGlobalError : public GlobalErrorWithStandardBubble {
 public:
  explicit ExtensionGlobalError(ExtensionErrorUI::Delegate* delegate)
      : delegate_(delegate) {}

 private:
  // GlobalError overrides:
  bool HasMenuItem() override { return false; }

  int MenuItemCommandID() override {
    NOTREACHED();
    return 0;
  }

  std::u16string MenuItemLabel() override {
    NOTREACHED();
    return {};
  }

  void ExecuteMenuItem(Browser* browser) override { NOTREACHED(); }

  std::u16string GetBubbleViewTitle() override {
    return GenerateTitle(delegate_->GetBlocklistedExtensions());
  }

  std::vector<std::u16string> GetBubbleViewMessages() override {
    return GenerateMessage(delegate_->GetBlocklistedExtensions(),
                           delegate_->GetContext());
  }

  std::u16string GetBubbleViewAcceptButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_OK);
  }

  std::u16string GetBubbleViewCancelButtonLabel() override { return {}; }

  std::u16string GetBubbleViewDetailsButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_DETAILS);
  }

  void OnBubbleViewDidClose(Browser* browser) override {
    delegate_->OnAlertClosed();
  }

  void BubbleViewAcceptButtonPressed(Browser* browser) override {
    delegate_->OnAlertAccept();
  }

  void BubbleViewCancelButtonPressed(Browser* browser) override {
    NOTREACHED();
  }

  void BubbleViewDetailsButtonPressed(Browser* browser) override {
    delegate_->OnAlertDetails();
  }

  ExtensionErrorUI::Delegate* delegate_;

  ExtensionGlobalError(const ExtensionGlobalError&) = delete;
  ExtensionGlobalError& operator=(const ExtensionGlobalError&) = delete;
};

ExtensionErrorUIDefault::ExtensionErrorUIDefault(
    ExtensionErrorUI::Delegate* delegate)
    : profile_(Profile::FromBrowserContext(delegate->GetContext())),
      global_error_(std::make_unique<ExtensionGlobalError>(delegate)) {}

ExtensionErrorUIDefault::~ExtensionErrorUIDefault() = default;

bool ExtensionErrorUIDefault::ShowErrorInBubbleView() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return false;

  browser_ = browser;
  global_error_->ShowBubbleView(browser);
  return true;
}

void ExtensionErrorUIDefault::ShowExtensions() {
  DCHECK(browser_);
  chrome::ShowExtensions(browser_, std::string());
}

void ExtensionErrorUIDefault::Close() {
  if (global_error_->HasShownBubbleView()) {
    // Will end up calling into |global_error_|->OnBubbleViewDidClose,
    // possibly synchronously.
    global_error_->GetBubbleView()->CloseBubbleView();
  }
}

GlobalErrorWithStandardBubble* ExtensionErrorUIDefault::GetErrorForTesting() {
  return global_error_.get();
}

}  // namespace extensions

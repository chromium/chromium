// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

std::u16string GenerateTitle(const ExtensionSet& forbidden,
                             bool item_blocked_by_policy_exists,
                             int extension_count,
                             int app_count) {
  // If |item_blocked_by_policy_exists| is true, this ignores the case that
  // there may be a mixture of enterprise and blocklisted items in |forbidden|.
  // The case does happen but is rare. In addition, this assumes that all policy
  // blocked items are extensions only.
  if (item_blocked_by_policy_exists) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_POLICY_BLOCKED_EXTENSION_ALERT_TITLE, extension_count + app_count);
  }

  // Otherwise, the extensions/apps are marked as malware because all other
  // blocklist reasons are not included in alerts yet.
  if ((app_count > 1) && (extension_count > 1)) {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_AND_APP_ALERT_TITLE);
  }
  if (app_count > 0) {
    return l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, app_count);
  }
  return l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE,
                                          extension_count);
}

std::vector<std::u16string> GenerateEnterpriseMessage(
    const ExtensionSet& forbidden) {
  std::vector<std::u16string> message;
  message.reserve(forbidden.size() + 1);
  // This assumes that all policy blocked items are extensions only.
  if (forbidden.size() > 1) {
    message.push_back(l10n_util::GetStringUTF16(
        IDS_POLICY_BLOCKED_EXTENSIONS_ALERT_ITEM_TITLE));
    for (const auto& extension : forbidden) {
      message.push_back(l10n_util::GetStringFUTF16(
          IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM,
          util::GetFixupExtensionNameForUIDisplay(extension->name())));
    }
  } else {
    message.push_back(l10n_util::GetStringFUTF16(
        IDS_POLICY_BLOCKED_EXTENSION_ALERT_ITEM_DETAIL,
        util::GetFixupExtensionNameForUIDisplay(
            forbidden.begin()->get()->name())));
  }
  return message;
}

std::vector<std::u16string> GenerateMessage(
    const ExtensionSet& forbidden,
    bool item_blocked_by_policy_exists) {
  std::vector<std::u16string> message;
  message.reserve(forbidden.size());

  // Currently, this ignores the case where there may be an extension that is
  // blockedlisted by enterprise and another extension blocklisted by Safe
  // Browsing.
  if (item_blocked_by_policy_exists) {
    return GenerateEnterpriseMessage(forbidden);
  }

  if (forbidden.size() == 1) {
    message.push_back(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_MALWARE,
                                   util::GetFixupExtensionNameForUIDisplay(
                                       forbidden.begin()->get()->name())));
    return message;
  }
  message.push_back(l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_ALERT_ITEM_BLOCKLISTED_MALWARE_TITLE));
  for (const auto& extension : forbidden) {
    message.push_back(l10n_util::GetStringFUTF16(
        IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM,
        util::GetFixupExtensionNameForUIDisplay(extension->name())));
  }
  return message;
}

}  // namespace

class ExtensionGlobalError : public GlobalErrorWithStandardBubble {
 public:
  explicit ExtensionGlobalError(ExtensionErrorUI::Delegate* delegate)
      : delegate_(delegate),
        management_policy_(
            ExtensionSystem::Get(delegate->GetContext())->management_policy()) {
    for (const auto& extension : delegate_->GetBlocklistedExtensions()) {
      if (extension->is_app()) {
        app_count_++;
      } else {
        extension_count_++;
      }
      if (management_policy_ &&
          !management_policy_->UserMayLoad(extension.get(),
                                           nullptr /*=ignore error */)) {
        item_blocked_by_policy_exists_ = true;
      }
    }
  }

  void SetManagementPolicy(ManagementPolicy* management_policy) {
    management_policy_ = management_policy;

    // Since the |management_policy_| may be set to something new,
    // |item_blocked_by_policy_exists_| may also need to be updated.
    if (management_policy_) {
      for (const auto& extension : delegate_->GetBlocklistedExtensions()) {
        if (!management_policy_->UserMayLoad(extension.get(),
                                             nullptr /*=ignore error */)) {
          item_blocked_by_policy_exists_ = true;
          break;
        }
      }
    }
  }

 private:
  // GlobalError overrides:
  bool HasMenuItem() override { return false; }

  int MenuItemCommandID() override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  std::u16string MenuItemLabel() override {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

  void ExecuteMenuItem(Browser* browser) override { NOTREACHED_IN_MIGRATION(); }

  std::u16string GetBubbleViewTitle() override {
    return GenerateTitle(delegate_->GetBlocklistedExtensions(),
                         item_blocked_by_policy_exists_, extension_count_,
                         app_count_);
  }

  std::vector<std::u16string> GetBubbleViewMessages() override {
    return GenerateMessage(delegate_->GetBlocklistedExtensions(),
                           item_blocked_by_policy_exists_);
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
    // Even though there is no cancel button, users can still cancel the dialog
    // by pressing escape.
    delegate_->OnAlertClosed();
  }

  base::WeakPtr<GlobalErrorWithStandardBubble> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BubbleViewDetailsButtonPressed(Browser* browser) override {
    delegate_->OnAlertDetails();
  }

  raw_ptr<ExtensionErrorUI::Delegate> delegate_;
  raw_ptr<ManagementPolicy, DanglingUntriaged> management_policy_;
  int app_count_ = 0;
  int extension_count_ = 0;
  bool item_blocked_by_policy_exists_ = false;
  base::WeakPtrFactory<ExtensionGlobalError> weak_ptr_factory_{this};

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
  chrome::ShowExtensions(browser_);
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

void ExtensionErrorUIDefault::SetManagementPolicyForTesting(
    ManagementPolicy* management_policy) {
  global_error_->SetManagementPolicy(management_policy);
}

}  // namespace extensions

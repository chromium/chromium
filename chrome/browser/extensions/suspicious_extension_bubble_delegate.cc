// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suspicious_extension_bubble_delegate.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionMessageBubbleController;

namespace {

// Whether the user has been notified about extension being wiped out.
const char kWipeoutAcknowledged[] = "ack_wiped";

}  // namespace

namespace extensions {

namespace {

base::LazyInstance<std::set<Profile*>>::Leaky g_suspicious_extension_shown =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SuspiciousExtensionBubbleDelegate::SuspiciousExtensionBubbleDelegate(
    Profile* profile)
    : extensions::ExtensionMessageBubbleController::Delegate(profile),
      profile_(profile) {
  set_acknowledged_flag_pref_name(kWipeoutAcknowledged);
}

SuspiciousExtensionBubbleDelegate::~SuspiciousExtensionBubbleDelegate() {
}

bool SuspiciousExtensionBubbleDelegate::ShouldIncludeExtension(
    const extensions::Extension* extension) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(
      profile());
  if (!prefs->IsExtensionDisabled(extension->id()))
    return false;

  int disable_reasons = prefs->GetDisableReasons(extension->id());
  if (disable_reasons & extensions::disable_reason::DISABLE_NOT_VERIFIED)
    return !HasBubbleInfoBeenAcknowledged(extension->id());

  return false;
}

void SuspiciousExtensionBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void SuspiciousExtensionBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  // This bubble solicits no action from the user. Or as Nimoy would have it:
  // "Well, my work here is done".
}

std::u16string SuspiciousExtensionBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNSUPPORTED_DISABLED_TITLE);
}

std::u16string SuspiciousExtensionBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  int message_id = extension_count == 1 ?
      IDS_EXTENSIONS_SINGLE_UNSUPPORTED_DISABLED_BODY :
      IDS_EXTENSIONS_MULTIPLE_UNSUPPORTED_DISABLED_BODY;
  return l10n_util::GetStringFUTF16(
      message_id, l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
}

std::u16string SuspiciousExtensionBubbleDelegate::GetOverflowText(
    const std::u16string& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_DISABLED_AND_N_MORE,
            overflow_count);
}

GURL SuspiciousExtensionBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kRemoveNonCWSExtensionURL);
}

std::u16string SuspiciousExtensionBubbleDelegate::GetActionButtonLabel() const {
  return std::u16string();
}

std::u16string SuspiciousExtensionBubbleDelegate::GetDismissButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNSUPPORTED_DISABLED_BUTTON);
}

bool SuspiciousExtensionBubbleDelegate::ShouldCloseOnDeactivate() const {
  return false;
}

bool SuspiciousExtensionBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  DCHECK_LE(1u, extensions.size());
  return !g_suspicious_extension_shown.Get().count(profile_);
}

void SuspiciousExtensionBubbleDelegate::OnShown(
    const ExtensionIdList& extensions) {
  DCHECK_LE(1u, extensions.size());
  DCHECK(!g_suspicious_extension_shown.Get().count(profile_));
  g_suspicious_extension_shown.Get().insert(profile_);
}

void SuspiciousExtensionBubbleDelegate::OnAction() {
  // We clear the profile set because the user chooses to remove or disable the
  // extension. Thus if that extension or another takes effect, it is worth
  // mentioning to the user (ShouldShow() would return true) because it is
  // contrary to the user's choice.
  g_suspicious_extension_shown.Get().clear();
}

void SuspiciousExtensionBubbleDelegate::ClearProfileSetForTesting() {
  g_suspicious_extension_shown.Get().clear();
}

bool SuspiciousExtensionBubbleDelegate::ShouldShowExtensionList() const {
  return true;
}

bool SuspiciousExtensionBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return false;
}

bool SuspiciousExtensionBubbleDelegate::SupportsPolicyIndicator() {
  return false;
}

}  // namespace extensions

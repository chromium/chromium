// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/proxy_overridden_bubble_delegate.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The minimum time to wait (since the extension was installed) before notifying
// the user about it.
const int kDaysSinceInstallMin = 7;

// Whether the user has been notified about extension overriding the proxy.
const char kProxyBubbleAcknowledged[] = "ack_proxy_bubble";

base::LazyInstance<std::set<Profile*>>::Leaky g_proxy_overridden_shown =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ProxyOverriddenBubbleDelegate::ProxyOverriddenBubbleDelegate(Profile* profile)
    : ExtensionMessageBubbleController::Delegate(profile), profile_(profile) {
  set_acknowledged_flag_pref_name(kProxyBubbleAcknowledged);
}

ProxyOverriddenBubbleDelegate::~ProxyOverriddenBubbleDelegate() {}

bool ProxyOverriddenBubbleDelegate::ShouldIncludeExtension(
    const Extension* extension) {
  if (!extension_id_.empty() && extension_id_ != extension->id())
    return false;  // Only one extension can be controlling the proxy at a time.

  const Extension* overriding = GetExtensionOverridingProxy(profile());
  if (!overriding || overriding != extension)
    return false;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  base::TimeDelta since_install =
      base::Time::Now() - prefs->GetInstallTime(extension->id());
  if (since_install.InDays() < kDaysSinceInstallMin)
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension->id()))
    return false;

  // Found the only extension; restrict to this one.
  extension_id_ = extension->id();

  return true;
}

void ProxyOverriddenBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void ProxyOverriddenBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service()->DisableExtension(list[i], disable_reason::DISABLE_USER_ACTION);
}

std::u16string ProxyOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_PROXY_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

std::u16string ProxyOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  if (anchored_to_browser_action) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE_EXTENSION_SPECIFIC);
  } else {
    const Extension* extension = registry()->GetExtensionById(
            extension_id_, ExtensionRegistry::EVERYTHING);
    // If the bubble is about to show, the extension should certainly exist.
    CHECK(extension);
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE,
        base::UTF8ToUTF16(extension->name()));
  }
}

std::u16string ProxyOverriddenBubbleDelegate::GetOverflowText(
    const std::u16string& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return std::u16string();
}

GURL ProxyOverriddenBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

std::u16string ProxyOverriddenBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

std::u16string ProxyOverriddenBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool ProxyOverriddenBubbleDelegate::ShouldCloseOnDeactivate() const {
  return false;
}

bool ProxyOverriddenBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  DCHECK_EQ(1u, extensions.size());
  return !g_proxy_overridden_shown.Get().count(profile_);
}

void ProxyOverriddenBubbleDelegate::OnShown(const ExtensionIdList& extensions) {
  DCHECK_EQ(1u, extensions.size());
  DCHECK(!g_proxy_overridden_shown.Get().count(profile_));
  g_proxy_overridden_shown.Get().insert(profile_);
}

void ProxyOverriddenBubbleDelegate::OnAction() {
  // We clear the profile set because the user chooses to remove or disable the
  // extension. Thus if that extension or another takes effect, it is worth
  // mentioning to the user (ShouldShow() would return true) because it is
  // contrary to the user's choice.
  g_proxy_overridden_shown.Get().clear();
}

void ProxyOverriddenBubbleDelegate::ClearProfileSetForTesting() {
  g_proxy_overridden_shown.Get().clear();
}

bool ProxyOverriddenBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool ProxyOverriddenBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

bool ProxyOverriddenBubbleDelegate::SupportsPolicyIndicator() {
  return true;
}

}  // namespace extensions

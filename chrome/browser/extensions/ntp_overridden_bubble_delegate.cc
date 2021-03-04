// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Whether existing NTP extensions have been automatically acknowledged.
const char kDidAcknowledgeExistingNtpExtensions[] =
    "ack_existing_ntp_extensions";

// Whether to acknowledge existing extensions overriding the NTP for the active
// profile. Active on MacOS to rollout the NTP bubble without prompting for
// previously-installed extensions.
bool g_acknowledge_existing_extensions =
#if defined(OS_MAC)
    true;
#else
    false;
#endif

base::LazyInstance<std::set<std::pair<Profile*, std::string>>>::Leaky
    g_ntp_overridden_shown = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

const char NtpOverriddenBubbleDelegate::kNtpBubbleAcknowledged[] =
    "ack_ntp_bubble";

NtpOverriddenBubbleDelegate::NtpOverriddenBubbleDelegate(Profile* profile)
    : extensions::ExtensionMessageBubbleController::Delegate(profile),
      profile_(profile) {
  set_acknowledged_flag_pref_name(kNtpBubbleAcknowledged);
}

NtpOverriddenBubbleDelegate::~NtpOverriddenBubbleDelegate() {}

// static
void NtpOverriddenBubbleDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDidAcknowledgeExistingNtpExtensions, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
}

// static
void NtpOverriddenBubbleDelegate::MaybeAcknowledgeExistingNtpExtensions(
    Profile* profile) {
  if (!g_acknowledge_existing_extensions)
    return;

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  PrefService* profile_prefs = profile->GetPrefs();
  // Only acknowledge existing extensions once per profile.
  if (profile_prefs->GetBoolean(kDidAcknowledgeExistingNtpExtensions))
    return;

  profile_prefs->SetBoolean(kDidAcknowledgeExistingNtpExtensions, true);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  for (const auto& extension : registry->enabled_extensions()) {
    const URLOverrides::URLOverrideMap& overrides =
        URLOverrides::GetChromeURLOverrides(extension.get());
    if (overrides.find(chrome::kChromeUINewTabHost) != overrides.end()) {
      prefs->UpdateExtensionPref(extension->id(), kNtpBubbleAcknowledged,
                                 std::make_unique<base::Value>(true));
    }
  }
}

bool NtpOverriddenBubbleDelegate::ShouldIncludeExtension(
    const extensions::Extension* extension) {
  if (!extension_id_.empty() && extension_id_ != extension->id())
    return false;

  GURL url(chrome::kChromeUINewTabURL);
  if (!ExtensionWebUI::HandleChromeURLOverride(&url, profile()))
    return false;  // No override for newtab found.

  if (extension->id() != url.host_piece())
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension->id()))
    return false;

  extension_id_ = extension->id();
  return true;
}

void NtpOverriddenBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void NtpOverriddenBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service()->DisableExtension(
        list[i], extensions::disable_reason::DISABLE_USER_ACTION);
  }
}

base::string16 NtpOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_NTP_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

base::string16 NtpOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  base::string16 body =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_NTP_CONTROLLED_FIRST_LINE);
  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);
  return body;
}

base::string16 NtpOverriddenBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return base::string16();
}

GURL NtpOverriddenBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

base::string16 NtpOverriddenBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

base::string16 NtpOverriddenBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool NtpOverriddenBubbleDelegate::ShouldCloseOnDeactivate() const {
  return true;
}

bool NtpOverriddenBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool NtpOverriddenBubbleDelegate::ShouldHighlightExtensions() const {
  return false;
}

bool NtpOverriddenBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

bool NtpOverriddenBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  DCHECK_EQ(1u, extensions.size());
  return !g_ntp_overridden_shown.Get().count(
      std::make_pair(profile_, extensions[0]));
}

void NtpOverriddenBubbleDelegate::OnShown(const ExtensionIdList& extensions) {
  DCHECK_EQ(1u, extensions.size());
  DCHECK(!g_ntp_overridden_shown.Get().count(
      std::make_pair(profile_, extensions[0])));
  g_ntp_overridden_shown.Get().insert(std::make_pair(profile_, extensions[0]));
}

void NtpOverriddenBubbleDelegate::OnAction() {
  // We clear the profile set because the user chooses to remove or disable the
  // extension. Thus if that extension or another takes effect, it is worth
  // mentioning to the user (ShouldShow() would return true) because it is
  // contrary to the user's choice.
  g_ntp_overridden_shown.Get().clear();
}

void NtpOverriddenBubbleDelegate::ClearProfileSetForTesting() {
  g_ntp_overridden_shown.Get().clear();
}

void NtpOverriddenBubbleDelegate::LogExtensionCount(size_t count) {
}

void NtpOverriddenBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionOverrideBubble.NtpOverriddenUserSelection",
      action,
      ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

bool NtpOverriddenBubbleDelegate::SupportsPolicyIndicator() {
  return true;
}

void NtpOverriddenBubbleDelegate::
    set_acknowledge_existing_extensions_for_testing(
        bool acknowledge_existing_extensions) {
  g_acknowledge_existing_extensions = acknowledge_existing_extensions;
}

}  // namespace extensions

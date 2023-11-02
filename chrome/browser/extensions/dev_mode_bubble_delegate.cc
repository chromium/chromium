// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_delegate.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

base::LazyInstance<std::set<Profile*>>::Leaky g_dev_mode_shown =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DevModeBubbleDelegate::DevModeBubbleDelegate(Profile* profile)
    : ExtensionMessageBubbleController::Delegate(profile), profile_(profile) {}

DevModeBubbleDelegate::~DevModeBubbleDelegate() {
}

bool DevModeBubbleDelegate::ShouldIncludeExtension(const Extension* extension) {
  return (extension->location() == mojom::ManifestLocation::kUnpacked ||
          extension->location() == mojom::ManifestLocation::kCommandLine);
}

void DevModeBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
}

void DevModeBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service()->DisableExtension(list[i], disable_reason::DISABLE_USER_ACTION);
}

std::u16string DevModeBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_TITLE);
}

std::u16string DevModeBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_BODY);
}

std::u16string DevModeBubbleDelegate::GetOverflowText(
    const std::u16string& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_DISABLED_AND_N_MORE,
            overflow_count);
}

std::u16string DevModeBubbleDelegate::GetLearnMoreLabel() const {
  return std::u16string();
}

GURL DevModeBubbleDelegate::GetLearnMoreUrl() const {
  return GURL();
}

std::u16string DevModeBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DISABLE);
}

std::u16string DevModeBubbleDelegate::GetDismissButtonLabel() const {
  return std::u16string();
}

bool DevModeBubbleDelegate::ShouldCloseOnDeactivate() const {
  return false;
}

bool DevModeBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  DCHECK_LE(1u, extensions.size());
  return !g_dev_mode_shown.Get().count(profile_->GetOriginalProfile());
}

void DevModeBubbleDelegate::OnShown(const ExtensionIdList& extensions) {
  DCHECK_LE(1u, extensions.size());
  DCHECK(!g_dev_mode_shown.Get().count(profile_));
  g_dev_mode_shown.Get().insert(profile_->GetOriginalProfile());
}

void DevModeBubbleDelegate::OnAction() {}

void DevModeBubbleDelegate::ClearProfileSetForTesting() {
  g_dev_mode_shown.Get().clear();
}

bool DevModeBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool DevModeBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

bool DevModeBubbleDelegate::SupportsPolicyIndicator() {
  return false;
}

}  // namespace extensions

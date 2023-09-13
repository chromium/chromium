// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_bubble_delegate.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

using ProfileSetMap = std::map<std::string, std::set<Profile*>>;
base::LazyInstance<ProfileSetMap>::Leaky g_settings_api_shown =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SettingsApiBubbleDelegate::SettingsApiBubbleDelegate(
    Profile* profile,
    SettingsApiOverrideType type)
    : ExtensionMessageBubbleController::Delegate(profile),
      type_(type),
      profile_(profile) {
  set_acknowledged_flag_pref_name(kAcknowledgedPreference);
}

const char SettingsApiBubbleDelegate::kAcknowledgedPreference[] =
    "ack_settings_bubble";

SettingsApiBubbleDelegate::~SettingsApiBubbleDelegate() {}

bool SettingsApiBubbleDelegate::ShouldIncludeExtension(
    const Extension* extension) {
  // If the browser is showing the 'Chrome crashed' infobar, it won't be showing
  // the startup pages, so there's no point in showing the bubble now.
  if (type_ == BUBBLE_TYPE_STARTUP_PAGES &&
      ExitTypeService::GetLastSessionExitType(profile()) == ExitType::kCrashed)
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension->id()))
    return false;

  const Extension* override = nullptr;
  switch (type_) {
    case extensions::BUBBLE_TYPE_HOME_PAGE:
      override = extensions::GetExtensionOverridingHomepage(profile());
      break;
    case extensions::BUBBLE_TYPE_STARTUP_PAGES:
      override = extensions::GetExtensionOverridingStartupPages(profile());
      break;
    case extensions::BUBBLE_TYPE_SEARCH_ENGINE:
      override = extensions::GetExtensionOverridingSearchEngine(profile());
      break;
  }

  if (!override || override != extension)
    return false;

  extension_id_ = extension->id();
  return true;
}

void SettingsApiBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void SettingsApiBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service()->DisableExtension(list[i], disable_reason::DISABLE_USER_ACTION);
  }
}

std::u16string SettingsApiBubbleDelegate::GetTitle() const {
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_HOME_PAGE_BUBBLE);
    case BUBBLE_TYPE_STARTUP_PAGES:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_STARTUP_PAGES_BUBBLE);
    case BUBBLE_TYPE_SEARCH_ENGINE:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_SEARCH_ENGINE_BUBBLE);
  }
  NOTREACHED();
  return std::u16string();
}

std::u16string SettingsApiBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  const Extension* extension =
      registry()->GetExtensionById(extension_id_, ExtensionRegistry::ENABLED);
  const SettingsOverrides* settings =
      extension ? SettingsOverrides::Get(extension) : nullptr;
  if (!extension || !settings) {
    NOTREACHED();
    return std::u16string();
  }

  bool home_change = settings->homepage.has_value();
  bool startup_change = !settings->startup_pages.empty();
  bool search_change = settings->search_engine.has_value();

  int first_line_id = 0;
  int second_line_id = 0;

  std::u16string body;
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE;
      if (startup_change && search_change) {
        second_line_id =
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_SEARCH;
      } else if (startup_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
      } else if (search_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
      }
      break;
    case BUBBLE_TYPE_STARTUP_PAGES:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_START_PAGES_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_START_PAGES;
      if (home_change && search_change) {
        second_line_id =
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_AND_SEARCH;
      } else if (home_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE;
      } else if (search_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
      }
      break;
    case BUBBLE_TYPE_SEARCH_ENGINE:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_SEARCH_ENGINE_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_SEARCH_ENGINE;
      if (startup_change && home_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_HOME;
      else if (startup_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
      else if (home_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE;
      break;
  }
  DCHECK_NE(0, first_line_id);
  body = anchored_to_browser_action ?
      l10n_util::GetStringUTF16(first_line_id) :
      l10n_util::GetStringFUTF16(first_line_id,
                                 base::UTF8ToUTF16(extension->name()));
  if (second_line_id)
    body += l10n_util::GetStringUTF16(second_line_id);

  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);

  return body;
}

std::u16string SettingsApiBubbleDelegate::GetOverflowText(
    const std::u16string& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return std::u16string();
}

GURL SettingsApiBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

std::u16string SettingsApiBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

std::u16string SettingsApiBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool SettingsApiBubbleDelegate::ShouldCloseOnDeactivate() const {
  // Startup bubbles tend to get lost in the focus storm that happens on
  // startup. Other types should dismiss on focus loss.
  return type_ != BUBBLE_TYPE_STARTUP_PAGES;
}

bool SettingsApiBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  DCHECK_EQ(1u, extensions.size());
  return !g_settings_api_shown.Get()[GetKey()].count(profile_);
}

void SettingsApiBubbleDelegate::OnShown(const ExtensionIdList& extensions) {
  DCHECK_EQ(1u, extensions.size());
  DCHECK(!g_settings_api_shown.Get()[GetKey()].count(profile_));
  g_settings_api_shown.Get()[GetKey()].insert(profile_);
}

void SettingsApiBubbleDelegate::OnAction() {
  // We clear the profile set because the user chooses to remove or disable the
  // extension. Thus if that extension or another takes effect, it is worth
  // mentioning to the user (ShouldShow() would return true) because it is
  // contrary to the user's choice.
  g_settings_api_shown.Get()[GetKey()].clear();
}

void SettingsApiBubbleDelegate::ClearProfileSetForTesting() {
  g_settings_api_shown.Get()[GetKey()].clear();
}

bool SettingsApiBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool SettingsApiBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

const char* SettingsApiBubbleDelegate::GetKey() const {
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      return "SettingsApiBubbleDelegate.HomePage";
    case BUBBLE_TYPE_STARTUP_PAGES:
      return "SettingsApiBubbleDelegate.StartupPages";
    case BUBBLE_TYPE_SEARCH_ENGINE:
      return "SettingsApiBubbleDelegate.SearchEngine";
  }
  NOTREACHED();
  return "";
}

bool SettingsApiBubbleDelegate::SupportsPolicyIndicator() {
  return true;
}

}  // namespace extensions

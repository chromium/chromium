// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/themes_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace {

// Make a name to pass to an extension helper.
std::string MakeName(int index) {
  return "faketheme" + base::NumberToString(index);
}

ThemeService* GetThemeService(Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile);
}

}  // namespace

namespace themes_helper {

std::string GetCustomTheme(int index) {
  return crx_file::id_util::GenerateId(MakeName(index));
}

std::string GetThemeID(Profile* profile) {
  return GetThemeService(profile)->GetThemeID();
}

bool UsingCustomTheme(Profile* profile) {
  return GetThemeID(profile) != ThemeService::kDefaultThemeID;
}

bool UsingDefaultTheme(Profile* profile) {
  return GetThemeService(profile)->UsingDefaultTheme();
}

bool UsingSystemTheme(Profile* profile) {
  return GetThemeService(profile)->UsingSystemTheme();
}

bool ThemeIsPendingInstall(Profile* profile, const std::string& id) {
  return SyncExtensionHelper::GetInstance()->
      IsExtensionPendingInstallForSync(profile, id);
}

void UseCustomTheme(Profile* profile, int index) {
  SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, MakeName(index), extensions::Manifest::TYPE_THEME);
}

void UseDefaultTheme(Profile* profile) {
  GetThemeService(profile)->UseDefaultTheme();
}

void UseSystemTheme(Profile* profile) {
  GetThemeService(profile)->UseSystemTheme();
}

// Helper function to let us bind this functionality into a base::Callback.
bool UsingSystemThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingSystemTheme();
}

// Helper function to let us bind this functionality into a base::Callback.
bool UsingDefaultThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingDefaultTheme();
}

}  // namespace themes_helper

ThemePendingInstallChecker::ThemePendingInstallChecker(Profile* profile,
                                                       const std::string& theme)
    : profile_(profile), theme_(theme) {
  // We'll check to see if the condition is met whenever the extension system
  // tries to contact the web store.
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
                 content::Source<Profile>(profile_));
}

ThemePendingInstallChecker::~ThemePendingInstallChecker() {
}

bool ThemePendingInstallChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pending theme to be '" << theme_ << "'";
  return themes_helper::ThemeIsPendingInstall(profile_, theme_);
}

void ThemePendingInstallChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED, type);
  CheckExitCondition();
}

ThemeConditionChecker::ThemeConditionChecker(
    Profile* profile,
    const std::string& debug_message,
    base::Callback<bool(ThemeService*)> exit_condition)
    : profile_(profile),
      debug_message_(debug_message),
      exit_condition_(exit_condition) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(GetThemeService(profile_)));
}

ThemeConditionChecker::~ThemeConditionChecker() {
}

bool ThemeConditionChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << debug_message_;
  return exit_condition_.Run(GetThemeService(profile_));
}

void ThemeConditionChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  CheckExitCondition();
}

SystemThemeChecker::SystemThemeChecker(Profile* profile)
    : ThemeConditionChecker(profile,
                            "Waiting until profile is using system theme",
                            base::Bind(&themes_helper::UsingSystemThemeFunc)) {}

DefaultThemeChecker::DefaultThemeChecker(Profile* profile)
    : ThemeConditionChecker(profile,
                            "Waiting until profile is using default theme",
                            base::Bind(&themes_helper::UsingDefaultThemeFunc)) {
}

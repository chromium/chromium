// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/themes_helper.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest.h"

namespace {

// Make a name to pass to an extension helper.
std::string MakeName(int index) {
  return "faketheme" + base::NumberToString(index);
}

ThemeService* GetThemeService(Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile);
}

bool UsingSystemThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingSystemTheme();
}

bool UsingDefaultThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingDefaultTheme();
}

bool UsingCustomThemeFunc(ThemeService* theme_service) {
  return theme_service->GetThemeID() != ThemeHelper::kDefaultThemeID;
}

}  // namespace

namespace themes_helper {

bool IsSystemThemeDistinctFromDefaultTheme(Profile* profile) {
  return GetThemeService(profile)->IsSystemThemeDistinctFromDefaultTheme();
}

std::string GetCustomTheme(int index) {
  return crx_file::id_util::GenerateId(MakeName(index));
}

std::string GetThemeID(Profile* profile) {
  return GetThemeService(profile)->GetThemeID();
}

bool UsingCustomTheme(Profile* profile) {
  return UsingCustomThemeFunc(GetThemeService(profile));
}

bool UsingDefaultTheme(Profile* profile) {
  return UsingDefaultThemeFunc(GetThemeService(profile));
}

bool UsingSystemTheme(Profile* profile) {
  return UsingSystemThemeFunc(GetThemeService(profile));
}

bool ThemeIsPendingInstall(Profile* profile, const std::string& id) {
  return SyncExtensionHelper::GetInstance()->IsExtensionPendingInstallForSync(
      profile, id);
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

}  // namespace themes_helper

ThemePendingInstallChecker::ThemePendingInstallChecker(Profile* profile,
                                                       const std::string& theme)
    : profile_(profile), theme_(theme) {
  CHECK(extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->updater());
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->updater()
      ->SetUpdatingStartedCallbackForTesting(
          base::BindRepeating(&ThemePendingInstallChecker::CheckExitCondition,
                              weak_ptr_factory_.GetWeakPtr()));
}

ThemePendingInstallChecker::~ThemePendingInstallChecker() = default;

bool ThemePendingInstallChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pending theme to be '" << *theme_ << "'";
  return themes_helper::ThemeIsPendingInstall(profile_, *theme_);
}

ThemeConditionChecker::ThemeConditionChecker(
    Profile* profile,
    const std::string& debug_message,
    const base::RepeatingCallback<bool(ThemeService*)>& exit_condition)
    : profile_(profile),
      debug_message_(debug_message),
      exit_condition_(exit_condition) {
  GetThemeService(profile_)->AddObserver(this);
}

ThemeConditionChecker::~ThemeConditionChecker() {
  GetThemeService(profile_)->RemoveObserver(this);
}

bool ThemeConditionChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << debug_message_;
  return exit_condition_.Run(GetThemeService(profile_));
}

void ThemeConditionChecker::OnThemeChanged() {
  CheckExitCondition();
}

SystemThemeChecker::SystemThemeChecker(Profile* profile)
    : ThemeConditionChecker(profile,
                            "Waiting until profile is using system theme",
                            base::BindRepeating(&UsingSystemThemeFunc)) {}

DefaultThemeChecker::DefaultThemeChecker(Profile* profile)
    : ThemeConditionChecker(profile,
                            "Waiting until profile is using default theme",
                            base::BindRepeating(&UsingDefaultThemeFunc)) {}

CustomThemeChecker::CustomThemeChecker(Profile* profile)
    : ThemeConditionChecker(profile,
                            "Waiting until profile is using a custom theme",
                            base::BindRepeating(&UsingCustomThemeFunc)) {}

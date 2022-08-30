// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_aura_linux.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/image/image.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/native_theme/native_theme_aura.h"

namespace {

class SystemThemeLinux : public CustomThemeSupplier {
 public:
  SystemThemeLinux(PrefService* pref_service, ui::SystemTheme system_theme);

  SystemThemeLinux(const SystemThemeLinux&) = delete;
  SystemThemeLinux& operator=(const SystemThemeLinux&) = delete;

  // Overridden from CustomThemeSupplier:
  void StartUsingTheme() override;
  void StopUsingTheme() override;
  bool GetColor(int id, SkColor* color) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  gfx::Image GetImageNamed(int id) const override;
  bool HasCustomImage(int id) const override;
  ui::NativeTheme* GetNativeTheme() const override;

 private:
  ~SystemThemeLinux() override;

  // This pointer is not owned by us.
  const raw_ptr<PrefService> pref_service_;
  const ui::SystemTheme system_theme_;
};

SystemThemeLinux::SystemThemeLinux(PrefService* pref_service,
                                   ui::SystemTheme system_theme)
    : CustomThemeSupplier(ThemeType::kNativeX11),
      pref_service_(pref_service),
      system_theme_(system_theme) {}

void SystemThemeLinux::StartUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, true);
  // Have the former theme notify its observers of change.
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void SystemThemeLinux::StopUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, false);
  // Have the former theme notify its observers of change.
  if (auto* linux_ui = ui::GetLinuxUi(system_theme_))
    linux_ui->GetNativeTheme()->NotifyOnNativeThemeUpdated();
}

bool SystemThemeLinux::GetColor(int id, SkColor* color) const {
  if (auto* linux_ui = ui::GetLinuxUi(system_theme_)) {
    return linux_ui->GetColor(
        id, color, pref_service_->GetBoolean(prefs::kUseCustomChromeFrame));
  }
  return false;
}

bool SystemThemeLinux::GetDisplayProperty(int id, int* result) const {
  if (auto* linux_ui = ui::GetLinuxUi(system_theme_))
    return linux_ui->GetDisplayProperty(id, result);
  return false;
}

gfx::Image SystemThemeLinux::GetImageNamed(int id) const {
  return gfx::Image();
}

bool SystemThemeLinux::HasCustomImage(int id) const {
  return false;
}

ui::NativeTheme* SystemThemeLinux::GetNativeTheme() const {
  if (auto* linux_ui = ui::GetLinuxUi(system_theme_)) {
    if (auto* native_theme = linux_ui->GetNativeTheme())
      return native_theme;
  }
  return CustomThemeSupplier::GetNativeTheme();
}

SystemThemeLinux::~SystemThemeLinux() = default;

}  // namespace

ThemeServiceAuraLinux::~ThemeServiceAuraLinux() = default;

ui::SystemTheme ThemeServiceAuraLinux::GetDefaultSystemTheme() const {
  return GetSystemThemeForProfile(profile());
}

void ThemeServiceAuraLinux::UseTheme(ui::SystemTheme system_theme) {
  if (system_theme == ui::SystemTheme::kDefault) {
    UseDefaultTheme();
    return;
  }
  if (ui::GetLinuxUi(system_theme)) {
    SetCustomDefaultTheme(
        new SystemThemeLinux(profile()->GetPrefs(), system_theme));
  }
}

void ThemeServiceAuraLinux::UseSystemTheme() {
  if (UsingSystemTheme())
    return;
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    if (auto* native_theme = linux_ui->GetNativeTheme()) {
      UseTheme(native_theme->system_theme());
      return;
    }
  }
  ThemeService::UseSystemTheme();
}

bool ThemeServiceAuraLinux::IsSystemThemeDistinctFromDefaultTheme() const {
  return true;
}

bool ThemeServiceAuraLinux::UsingSystemTheme() const {
  return GetThemeSupplier() &&
         GetThemeSupplier()->get_theme_type() ==
             ui::ColorProviderManager::ThemeInitializerSupplier::ThemeType::
                 kNativeX11;
}

void ThemeServiceAuraLinux::FixInconsistentPreferencesIfNeeded() {
  PrefService* prefs = profile()->GetPrefs();

  // When using the system theme, the theme ID should match the default. Give
  // precedence to the non-default theme specified.
  if (GetThemeID() != ThemeHelper::kDefaultThemeID &&
      prefs->GetBoolean(prefs::kUsesSystemTheme)) {
    prefs->SetBoolean(prefs::kUsesSystemTheme, false);
  }
}

// static
ui::SystemTheme ThemeServiceAuraLinux::GetSystemThemeForProfile(
    const Profile* profile) {
#if BUILDFLAG(IS_LINUX)
  // TODO(https://crbug.com/1317782): Add QT theme preference.
  bool use_system_theme =
      !profile || (!profile->IsChild() &&
                   profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme));
  if (use_system_theme)
    return ui::SystemTheme::kGtk;
#endif
  return ui::SystemTheme::kDefault;
}

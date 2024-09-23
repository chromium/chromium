// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_aura_linux.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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

ui::SystemTheme ValidateSystemTheme(ui::SystemTheme system_theme) {
  switch (system_theme) {
    case ui::SystemTheme::kDefault:
#if BUILDFLAG(IS_LINUX)
    case ui::SystemTheme::kGtk:
    case ui::SystemTheme::kQt:
#endif
      return system_theme;
    default:
      return ui::SystemTheme::kDefault;
  }
}

class SystemThemeLinux : public CustomThemeSupplier {
 public:
  SystemThemeLinux(PrefService* pref_service, ui::LinuxUiTheme* linux_ui_theme);

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

  // These pointers are not owned by us.
  const raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  const raw_ptr<ui::LinuxUiTheme> linux_ui_theme_;
};

SystemThemeLinux::SystemThemeLinux(PrefService* pref_service,
                                   ui::LinuxUiTheme* linux_ui_theme)
    : CustomThemeSupplier(ThemeType::kNativeX11),
      pref_service_(pref_service),
      linux_ui_theme_(linux_ui_theme) {}

void SystemThemeLinux::StartUsingTheme() {
  pref_service_->SetInteger(
      prefs::kSystemTheme,
      static_cast<int>(linux_ui_theme_->GetNativeTheme()->system_theme()));
  // Have the former theme notify its observers of change.
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void SystemThemeLinux::StopUsingTheme() {
  pref_service_->SetInteger(prefs::kSystemTheme,
                            static_cast<int>(ui::SystemTheme::kDefault));
  // Have the former theme notify its observers of change.
  linux_ui_theme_->GetNativeTheme()->NotifyOnNativeThemeUpdated();
}

bool SystemThemeLinux::GetColor(int id, SkColor* color) const {
  return linux_ui_theme_->GetColor(
      id, color, pref_service_->GetBoolean(prefs::kUseCustomChromeFrame));
}

bool SystemThemeLinux::GetDisplayProperty(int id, int* result) const {
  return linux_ui_theme_->GetDisplayProperty(id, result);
}

gfx::Image SystemThemeLinux::GetImageNamed(int id) const {
  return gfx::Image();
}

bool SystemThemeLinux::HasCustomImage(int id) const {
  return false;
}

ui::NativeTheme* SystemThemeLinux::GetNativeTheme() const {
  return linux_ui_theme_->GetNativeTheme();
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
  } else if (auto* linux_ui_theme = ui::GetLinuxUiTheme(system_theme)) {
    SetCustomDefaultTheme(base::MakeRefCounted<SystemThemeLinux>(
        profile()->GetPrefs(), linux_ui_theme));
  } else {
    return;
  }
}

void ThemeServiceAuraLinux::UseSystemTheme() {
  if (UsingSystemTheme()) {
    return;
  }
  if (auto* linux_ui_theme = ui::GetDefaultLinuxUiTheme()) {
    if (auto* native_theme = linux_ui_theme->GetNativeTheme()) {
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
             ui::ColorProviderKey::ThemeInitializerSupplier::ThemeType::
                 kNativeX11;
}

void ThemeServiceAuraLinux::FixInconsistentPreferencesIfNeeded() {
  PrefService* prefs = profile()->GetPrefs();

  // When using the system theme, the theme ID should match the default. Give
  // precedence to the non-default theme specified.
  if (GetThemeID() != ThemeHelper::kDefaultThemeID &&
      prefs->GetInteger(prefs::kSystemTheme) !=
          static_cast<int>(ui::SystemTheme::kDefault)) {
    prefs->SetInteger(prefs::kSystemTheme,
                      static_cast<int>(ui::SystemTheme::kDefault));
  }
}

ThemeService::BrowserColorScheme ThemeServiceAuraLinux::GetBrowserColorScheme()
    const {
  // If using the system theme (GTK or QT), always use the system color scheme
  // as well.  This prevents eg. setting the color scheme to light when the
  // system theme is dark, which may lead to white text on white backgrounds.
  if (UsingSystemTheme()) {
    return ThemeService::BrowserColorScheme::kSystem;
  }
  return ThemeService::GetBrowserColorScheme();
}

// static
ui::SystemTheme ThemeServiceAuraLinux::GetSystemThemeForProfile(
    const Profile* profile) {
  if (!profile || profile->IsChild()) {
    return ui::SystemTheme::kDefault;
  }
  return ValidateSystemTheme(static_cast<ui::SystemTheme>(
      profile->GetPrefs()->GetInteger(prefs::kSystemTheme)));
}

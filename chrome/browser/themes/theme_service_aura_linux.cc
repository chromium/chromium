// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_aura_linux.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme_aura.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace {

class SystemThemeLinux : public CustomThemeSupplier {
 public:
  explicit SystemThemeLinux(PrefService* pref_service);

  SystemThemeLinux(const SystemThemeLinux&) = delete;
  SystemThemeLinux& operator=(const SystemThemeLinux&) = delete;

  // Overridden from CustomThemeSupplier:
  void StartUsingTheme() override;
  void StopUsingTheme() override;
  bool GetColor(int id, SkColor* color) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  gfx::Image GetImageNamed(int id) const override;
  bool HasCustomImage(int id) const override;

 private:
  ~SystemThemeLinux() override;

  // This pointer is not owned by us.
  const raw_ptr<PrefService> pref_service_;
};

SystemThemeLinux::SystemThemeLinux(PrefService* pref_service)
    : CustomThemeSupplier(ThemeType::kNativeX11), pref_service_(pref_service) {}

void SystemThemeLinux::StartUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, true);
  // Have the former theme notify its observers of change.
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void SystemThemeLinux::StopUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, false);
  // Have the former theme notify its observers of change.
#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance())
    linux_ui->GetNativeTheme(nullptr)->NotifyOnNativeThemeUpdated();
#endif
}

bool SystemThemeLinux::GetColor(int id, SkColor* color) const {
#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    return linux_ui->GetColor(
        id, color, pref_service_->GetBoolean(prefs::kUseCustomChromeFrame));
  }
#endif
  return false;
}

bool SystemThemeLinux::GetDisplayProperty(int id, int* result) const {
#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance())
    return linux_ui->GetDisplayProperty(id, result);
#endif
  return false;
}

gfx::Image SystemThemeLinux::GetImageNamed(int id) const {
  return gfx::Image();
}

bool SystemThemeLinux::HasCustomImage(int id) const {
  return false;
}

SystemThemeLinux::~SystemThemeLinux() = default;

}  // namespace

ThemeServiceAuraLinux::~ThemeServiceAuraLinux() = default;

bool ThemeServiceAuraLinux::ShouldInitWithSystemTheme() const {
  return ShouldUseSystemThemeForProfile(profile());
}

void ThemeServiceAuraLinux::UseSystemTheme() {
  SetCustomDefaultTheme(new SystemThemeLinux(profile()->GetPrefs()));
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
bool ThemeServiceAuraLinux::ShouldUseSystemThemeForProfile(
    const Profile* profile) {
  return !profile || (!profile->IsChild() &&
                      profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme));
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/soda_installer_impl_chromeos.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "components/prefs/pref_service.h"
#include "components/soda/pref_names.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kSodaDlcName[] = "libsoda";
constexpr char kSodaEnglishUsDlcName[] = "libsoda-model-en-us";

}  // namespace

namespace speech {

SodaInstaller* SodaInstaller::GetInstance() {
  static base::NoDestructor<SodaInstallerImplChromeOS> instance;
  return instance.get();
}

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() = default;

SodaInstallerImplChromeOS::~SodaInstallerImplChromeOS() = default;

base::FilePath SodaInstallerImplChromeOS::GetSodaBinaryPath() const {
  return soda_lib_path_;
}

base::FilePath SodaInstallerImplChromeOS::GetLanguagePath() const {
  return language_path_;
}

void SodaInstallerImplChromeOS::InstallSoda(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return;

  // Clear cached path in case this is a reinstallation (path could
  // change).
  SetSodaBinaryPath(base::FilePath());

  soda_binary_installed_ = false;
  is_soda_downloading_ = true;
  soda_progress_ = 0.0;

  // Install SODA DLC.
  chromeos::DlcserviceClient::Get()->Install(
      kSodaDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnSodaInstalled,
                     base::Unretained(this)),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnSodaProgress,
                          base::Unretained(this)));
}

void SodaInstallerImplChromeOS::InstallLanguage(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return;

  // Clear cached path in case this is a reinstallation (path could
  // change).
  SetLanguagePath(base::FilePath());

  // TODO(crbug.com/1055150): Compare user's language to a list of
  // supported languages and map it to a DLC ID. For now, default to
  // installing the SODA English-US DLC.
  std::string user_language = prefs->GetString(prefs::kLiveCaptionLanguageCode);
  DCHECK_EQ(user_language, "en-US");

  language_installed_ = false;
  is_language_downloading_ = true;
  language_progress_ = 0.0;

  chromeos::DlcserviceClient::Get()->Install(
      kSodaEnglishUsDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnLanguageInstalled,
                     base::Unretained(this)),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnLanguageProgress,
                          base::Unretained(this)));
}

bool SodaInstallerImplChromeOS::IsSodaInstalled() const {
  DCHECK(base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption));
  return (soda_binary_installed_ && language_installed_) ||
         soda_installed_for_test_;
}

bool SodaInstallerImplChromeOS::IsLanguageInstalled(
    const std::string& locale_or_language) const {
  // TODO(crbug.com/1161569): SODA is only available for English right now.
  // Update this to check installation of language pack when available.
  return (l10n_util::GetLanguage(locale_or_language) == "en" &&
          language_installed_) ||
         soda_installed_for_test_;
}

void SodaInstallerImplChromeOS::UninstallSoda(PrefService* global_prefs) {
  soda_binary_installed_ = false;
  SetSodaBinaryPath(base::FilePath());
  chromeos::DlcserviceClient::Get()->Uninstall(
      kSodaDlcName, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), kSodaDlcName));
  language_installed_ = false;
  SetLanguagePath(base::FilePath());
  chromeos::DlcserviceClient::Get()->Uninstall(
      kSodaEnglishUsDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                     base::Unretained(this), kSodaEnglishUsDlcName));
  global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
}

void SodaInstallerImplChromeOS::SetSodaBinaryPath(base::FilePath new_path) {
  soda_lib_path_ = new_path;
}

void SodaInstallerImplChromeOS::SetLanguagePath(base::FilePath new_path) {
  language_path_ = new_path;
}

void SodaInstallerImplChromeOS::OnSodaInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    soda_binary_installed_ = true;
    SetSodaBinaryPath(base::FilePath(install_result.root_path));
    if (language_installed_) {
      NotifyOnSodaInstalled();
    }
  } else {
    soda_binary_installed_ = false;
    soda_progress_ = 0.0;
    NotifyOnSodaError();
  }
  is_soda_downloading_ = false;
}

void SodaInstallerImplChromeOS::OnLanguageInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    language_installed_ = true;
    SetLanguagePath(base::FilePath(install_result.root_path));
    if (soda_binary_installed_) {
      NotifyOnSodaInstalled();
    }
  } else {
    language_installed_ = false;
    language_progress_ = 0.0;
    NotifyOnSodaError();
  }
  is_language_downloading_ = false;
}

void SodaInstallerImplChromeOS::OnSodaProgress(double progress) {
  soda_progress_ = progress;
  OnSodaCombinedProgress();
}

void SodaInstallerImplChromeOS::OnLanguageProgress(double progress) {
  language_progress_ = progress;
  OnSodaCombinedProgress();
}

void SodaInstallerImplChromeOS::OnSodaCombinedProgress() {
  // TODO(crbug.com/1055150): Consider updating this implementation.
  // e.g.: (1) starting progress from 0% if we are downloading language
  // only (2) weighting download progress proportionally to DLC binary size.
  const double progress = (soda_progress_ + language_progress_) / 2;
  NotifyOnSodaProgress(int{100 * progress});
}

void SodaInstallerImplChromeOS::OnDlcUninstalled(const std::string& dlc_id,
                                                 const std::string& err) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to uninstall DLC " << dlc_id << ". Error: " << err;
  }
}

}  // namespace speech

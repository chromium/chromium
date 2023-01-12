// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer_impl.h"

#include <map>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/update_client/crx_update_item.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace speech {

SodaInstallerImpl::SodaInstallerImpl() = default;

SodaInstallerImpl::~SodaInstallerImpl() {
  component_updater_observation_.Reset();
}

base::FilePath SodaInstallerImpl::GetSodaBinaryPath() const {
  DLOG(FATAL) << "GetSodaBinaryPath not supported on this platform";
  return base::FilePath();
}

base::FilePath SodaInstallerImpl::GetLanguagePath(
    const std::string& language) const {
  DLOG(FATAL) << "GetLanguagePath not supported on this platform";
  return base::FilePath();
}

void SodaInstallerImpl::InstallSoda(PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  soda_binary_installed_ = false;
  is_soda_downloading_ = true;
  component_updater::RegisterSodaComponent(
      g_browser_process->component_updater(), global_prefs,
      base::BindOnce(&SodaInstallerImpl::OnSodaBinaryInstalled,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&component_updater::SodaComponentInstallerPolicy::
                         UpdateSodaComponentOnDemand));
  soda_binary_install_start_time_ = base::Time::Now();
  if (!component_updater_observation_.IsObservingSource(
          g_browser_process->component_updater())) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
}

void SodaInstallerImpl::InstallLanguage(const std::string& language,
                                        PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  speech::LanguageCode locale = speech::GetLanguageCode(language);
  language_pack_progress_.insert({locale, 0.0});
  SodaInstaller::RegisterLanguage(language, global_prefs);
  component_updater::RegisterSodaLanguageComponent(
      g_browser_process->component_updater(), language, global_prefs,
      base::BindOnce(&SodaInstallerImpl::OnSodaLanguagePackInstalled,
                     weak_factory_.GetWeakPtr()));

  language_pack_install_start_time_[locale] = base::Time::Now();

  if (!component_updater_observation_.IsObservingSource(
          g_browser_process->component_updater())) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
}

void SodaInstallerImpl::UninstallLanguage(const std::string& language,
                                          PrefService* global_prefs) {
  speech::LanguageCode language_code = speech::GetLanguageCode(language);
  if (language_code != speech::LanguageCode::kNone) {
    // Remove the language from the preference tracking installed language packs
    // and unregister the corresponding component from the component updater
    // service to remove the files and prevent future updates.
    SodaInstaller::UnregisterLanguage(language, global_prefs);
    const std::string crx_id = component_updater::
        SodaLanguagePackComponentInstallerPolicy::GetExtensionId(language_code);
    auto* component_updater_service = g_browser_process->component_updater();
    if (component_updater_service) {
      component_updater_service->UnregisterComponent(crx_id);
    }

    std::set<speech::LanguageCode>::iterator it =
        installed_languages_.find(language_code);
    if (it != installed_languages_.end()) {
      installed_languages_.erase(it);
    }
  }
}

std::vector<std::string> SodaInstallerImpl::GetAvailableLanguages() const {
  // TODO(crbug.com/1161569): SODA is only available for English right now.
  // Update this to check available languages.
  return {kUsEnglishLocale};
}

void SodaInstallerImpl::UninstallSoda(PrefService* global_prefs) {
  SodaInstaller::UnregisterLanguages(global_prefs);
  base::DeletePathRecursively(speech::GetSodaDirectory());
  base::DeletePathRecursively(speech::GetSodaLanguagePacksDirectory());
  global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());

  soda_binary_installed_ = false;
  is_soda_downloading_ = false;
  soda_installer_initialized_ = false;
  installed_languages_.clear();
  language_pack_progress_.clear();
}

void SodaInstallerImpl::OnEvent(Events event, const std::string& id) {
  if (!component_updater::SodaLanguagePackComponentInstallerPolicy::
           GetExtensionIds()
               .contains(id) &&
      id != component_updater::SodaComponentInstallerPolicy::GetExtensionId()) {
    return;
  }

  LanguageCode language_code = LanguageCode::kNone;
  if (id != component_updater::SodaComponentInstallerPolicy::GetExtensionId()) {
    language_code = GetLanguageCodeByComponentId(id);
    DCHECK_NE(language_code, LanguageCode::kNone);
  }

  switch (event) {
    case Events::COMPONENT_UPDATE_FOUND:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING: {
      update_client::CrxUpdateItem item;
      g_browser_process->component_updater()->GetComponentDetails(id, &item);
      downloading_components_[language_code] = item;

      if (language_code == LanguageCode::kNone &&
          !language_pack_progress_.empty()) {
        for (auto language : language_pack_progress_) {
          UpdateAndNotifyOnSodaProgress(language.first);
        }
      } else {
        UpdateAndNotifyOnSodaProgress(language_code);
      }
    } break;
    case Events::COMPONENT_UPDATE_ERROR:
      is_soda_downloading_ = false;

      if (language_code != LanguageCode::kNone) {
        language_pack_progress_.erase(language_code);
        base::UmaHistogramTimes(
            GetInstallationFailureTimeMetricForLanguagePack(language_code),
            base::Time::Now() -
                language_pack_install_start_time_[language_code]);

        base::UmaHistogramBoolean(
            GetInstallationResultMetricForLanguagePack(language_code), false);
      } else {
        base::UmaHistogramTimes(
            kSodaBinaryInstallationFailureTimeTaken,
            base::Time::Now() - soda_binary_install_start_time_);

        base::UmaHistogramBoolean(kSodaBinaryInstallationResult, false);
      }

      NotifyOnSodaInstallError(
          language_code, speech::SodaInstaller::ErrorCode::kUnspecifiedError);
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_ALREADY_UP_TO_DATE:
      // Do nothing.
      break;
  }
}

void SodaInstallerImpl::OnSodaBinaryInstalled() {
  soda_binary_installed_ = true;
  is_soda_downloading_ = false;
  for (LanguageCode language : installed_languages_) {
    NotifyOnSodaInstalled(language);
  }

  base::UmaHistogramTimes(kSodaBinaryInstallationSuccessTimeTaken,
                          base::Time::Now() - soda_binary_install_start_time_);
  base::UmaHistogramBoolean(kSodaBinaryInstallationResult, true);
}

void SodaInstallerImpl::OnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  installed_languages_.insert(language_code);
  language_pack_progress_.erase(language_code);

  if (soda_binary_installed_) {
    NotifyOnSodaInstalled(language_code);
  }

  base::UmaHistogramTimes(
      GetInstallationSuccessTimeMetricForLanguagePack(language_code),
      base::Time::Now() - language_pack_install_start_time_[language_code]);
  base::UmaHistogramBoolean(
      GetInstallationResultMetricForLanguagePack(language_code), true);
}

void SodaInstallerImpl::UpdateAndNotifyOnSodaProgress(
    speech::LanguageCode language_code) {
  int total_bytes = 0;
  int downloaded_bytes = 0;
  speech::LanguageCode soda_code = speech::LanguageCode::kNone;

  if (base::Contains(downloading_components_, soda_code)) {
    total_bytes += downloading_components_[soda_code].total_bytes;
    downloaded_bytes += downloading_components_[soda_code].downloaded_bytes;
  }

  if (language_code != soda_code) {
    total_bytes += downloading_components_[language_code].total_bytes;
    downloaded_bytes += downloading_components_[language_code].downloaded_bytes;
  }

  if (total_bytes == 0)
    return;

  DCHECK_LE(downloaded_bytes, total_bytes);
  int progress =
      100 * base::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                        0.0, 1.0);
  if (language_code != soda_code)
    language_pack_progress_[language_code] = progress;
  NotifyOnSodaProgress(language_code, progress);
}

}  // namespace speech

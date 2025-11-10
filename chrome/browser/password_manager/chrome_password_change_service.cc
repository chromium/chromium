// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// Shorten the name to spare line breaks. The code provides enough context
// already.
using Logger = password_manager::BrowserSavePasswordProgressLogger;

// Returns whether chrome switch for change password URLs is used.
bool HasChangePasswordUrlOverride() {
  return !password_manager::GetChangePasswordUrlOverrides().empty();
}

// Returns whether overridden change password URL matches with `url`.
GURL GetChangePasswordURLOverride(const GURL& url) {
  if (!HasChangePasswordUrlOverride()) {
    return GURL();
  }

  if (!url.is_valid()) {
    return GURL();
  }

  for (auto& override_url : password_manager::GetChangePasswordUrlOverrides()) {
    if (!override_url.is_valid() ||
        !affiliations::IsExtendedPublicSuffixDomainMatch(url, override_url,
                                                         {})) {
      continue;
    }
    return std::move(override_url);
  }
  return GURL();
}

std::string GetVariationConfigCountryCode() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return variation_service ? variation_service->GetLatestCountry()
                           : std::string();
}

optimization_guide::prefs::FeatureOptInState GetFeatureState(
    PrefService* pref_service) {
  return static_cast<optimization_guide::prefs::FeatureOptInState>(
      pref_service->GetInteger(
          optimization_guide::prefs::GetSettingEnabledPrefName(
              optimization_guide::UserVisibleFeatureKey::
                  kPasswordChangeSubmission)));
}

std::pair<std::unique_ptr<autofill::LogManager>,
          std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger>>
CreateLoggerPair(autofill::LogRouter* log_router) {
  std::unique_ptr<autofill::LogManager> log_manager;
  if (log_router && log_router->HasReceivers()) {
    log_manager = autofill::LogManager::Create(log_router, base::DoNothing());
  }

  std::unique_ptr<Logger> logger;
  if (log_manager && log_manager->IsLoggingActive()) {
    logger = std::make_unique<Logger>(log_manager.get());
  }
  return {std::move(log_manager), std::move(logger)};
}

}  // namespace

ChromePasswordChangeService::ChromePasswordChangeService(
    PrefService* pref_service,
    affiliations::AffiliationService* affiliation_service,
    OptimizationGuideKeyedService* optimization_keyed_service,
    password_manager::PasswordManagerSettingsService* settings_service,
    std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager,
    autofill::LogRouter* log_router)
    : pref_service_(pref_service),
      affiliation_service_(affiliation_service),
      optimization_keyed_service_(optimization_keyed_service),
      settings_service_(settings_service),
      feature_manager_(std::move(feature_manager)),
      log_router_(log_router) {}

ChromePasswordChangeService::~ChromePasswordChangeService() {
  CHECK(password_change_delegates_.empty());
}

bool ChromePasswordChangeService::IsPasswordChangeAvailable() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return GetGeneralAvailability() == PasswordChangeAvailability::kAvailable;
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePasswordChangeService::RecordLoginAttemptQuality(
    password_manager::LogInWithChangedPasswordOutcome login_outcome,
    const GURL& page_url) const {
#if BUILDFLAG(IS_ANDROID)
  return;
#else
  optimization_guide::ModelQualityLogsUploaderService* mqls_service =
      optimization_keyed_service_->GetModelQualityLogsUploaderService();
  if (mqls_service) {
    ModelQualityLogsUploader::RecordLoginAttemptQuality(mqls_service, page_url,
                                                        login_outcome);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromePasswordChangeService::IsPasswordChangeSupported(
    const GURL& url,
    const autofill::LanguageCode& page_language) const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  auto availability = GetPerSiteAvailability(url, page_language);
  base::UmaHistogramEnumeration("PasswordManager.PasswordChangeAvailability",
                                availability);

  return availability == PasswordChangeAvailability::kAvailable;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromePasswordChangeService::UserIsActivePasswordChangeUser() const {
  auto [log_manager, logger] = CreateLoggerPair(log_router_);

  // The feature becomes enabled when user accepts to change a compromised
  // password.
  if (!pref_service_ ||
      (GetFeatureState(pref_service_) !=
       optimization_guide::prefs::FeatureOptInState::kEnabled)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_USER_IS_NOT_ACTIVE);
    }
    return false;
  }
  return IsPasswordChangeAvailable();
}

void ChromePasswordChangeService::OfferPasswordChangeUi(
    password_manager::PasswordForm credentials,
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  GURL change_pwd_url = GetChangePasswordURLOverride(credentials.url);
  if (!change_pwd_url.is_valid()) {
    change_pwd_url =
        affiliation_service_->GetChangePasswordURL(credentials.url);
  }

  CHECK(change_pwd_url.is_valid());

  std::unique_ptr<PasswordChangeDelegate> delegate =
      std::make_unique<PasswordChangeDelegateImpl>(
          std::move(change_pwd_url), std::move(credentials),
          tabs::TabInterface::GetFromContents(web_contents));
  delegate->AddObserver(this);
  password_change_delegates_.push_back(std::move(delegate));
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_ANDROID)
}

PasswordChangeDelegate* ChromePasswordChangeService::GetPasswordChangeDelegate(
    content::WebContents* web_contents) {
  for (const auto& delegate : password_change_delegates_) {
    if (delegate->IsPasswordChangeOngoing(web_contents)) {
      return delegate.get();
    }
  }
  return nullptr;
}

void ChromePasswordChangeService::OnPasswordChangeStopped(
    PasswordChangeDelegate* delegate) {
  delegate->RemoveObserver(this);

  auto iter = std::ranges::find(password_change_delegates_, delegate,
                                &std::unique_ptr<PasswordChangeDelegate>::get);
  CHECK(iter != password_change_delegates_.end());

  std::unique_ptr<PasswordChangeDelegate> deleted_delegate = std::move(*iter);
  password_change_delegates_.erase(iter);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(deleted_delegate));
}

void ChromePasswordChangeService::Shutdown() {
  for (const auto& delegate : password_change_delegates_) {
    delegate->RemoveObserver(this);
  }
  password_change_delegates_.clear();
}

PasswordChangeAvailability ChromePasswordChangeService::GetGeneralAvailability()
    const {
  auto [log_manager, logger] = CreateLoggerPair(log_router_);

  if (HasChangePasswordUrlOverride()) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_OVERRIDDEN_BY_SWITCH);
    }
    return PasswordChangeAvailability::kAvailable;
  }

  // Password generation is disabled.
  if (!feature_manager_->IsGenerationEnabled()) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_GENERATION_UNAVAILABLE);
    }
    return PasswordChangeAvailability::kPasswordGenerationDisabled;
  }

  // User is not eligible.
  if (!optimization_keyed_service_ ||
      !optimization_keyed_service_->ShouldModelExecutionBeAllowedForUser()) {
    if (logger) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_MODEL_EXECUTION_NOT_ALLOWED);
    }
    return PasswordChangeAvailability::kModelExecutionNotAllowed;
  }

  // Chrome shouldn't offer to save password. Since during password change a
  // password is saved, it shouldn't be offered.
  if (!settings_service_ ||
      !settings_service_->IsSettingEnabled(
          password_manager::PasswordManagerSetting::kOfferToSavePasswords)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_SAVING_DISABLED);
    }
    return PasswordChangeAvailability::kPasswordSavingDisabled;
  }

  // The feature is disabled by enterprise policy.
  constexpr int kPolicyDisabled =
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable);
  if (pref_service_->GetInteger(
          optimization_guide::prefs::
              kAutomatedPasswordChangeEnterprisePolicyAllowed) ==
      kPolicyDisabled) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_DISABLED_BY_POLICY);
    }
    return PasswordChangeAvailability::kDisabledByPolicy;
  }

  if (!pref_service_->GetInteger(
          password_manager::prefs::kTotalPasswordsAvailableForAccount) &&
      !pref_service_->GetInteger(
          password_manager::prefs::kTotalPasswordsAvailableForProfile)) {
    return PasswordChangeAvailability::kNoSavedPasswords;
  }

  if (base::FeatureList::IsEnabled(
          password_manager::features::kThrottlePasswordChangeDialog) &&
      base::Time::Now() -
              pref_service_->GetTime(password_manager::prefs::
                                         kLastNegativePasswordChangeTimestamp) <
          password_manager::features::kPasswordChangeThrottleTime.Get()) {
    return PasswordChangeAvailability::kThrottled;
  }

  const bool result = base::FeatureList::IsEnabled(
      password_manager::features::kImprovedPasswordChangeService);
  if (logger) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_FEATURE_ENABLED, result);
  }

  return result ? PasswordChangeAvailability::kAvailable
                : PasswordChangeAvailability::kFeatureDisabled;
}

PasswordChangeAvailability ChromePasswordChangeService::GetPerSiteAvailability(
    const GURL& url,
    const autofill::LanguageCode& page_language) const {
  auto [log_manager, logger] = CreateLoggerPair(log_router_);

  auto general_availability = GetGeneralAvailability();
  if (general_availability != PasswordChangeAvailability::kAvailable) {
    return general_availability;
  }

  if (GetChangePasswordURLOverride(url).is_valid()) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_OVERRIDDEN_BY_SWITCH);
    }
    return PasswordChangeAvailability::kAvailable;
  }

  if (page_language != autofill::LanguageCode("en") &&
      page_language != autofill::LanguageCode("en-US") &&
      !base::FeatureList::IsEnabled(
          password_manager::features::kReduceRequirementsForPasswordChange)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_UNSUPPORTED_LANGUAGE);
    }
    return PasswordChangeAvailability::kUnsupportedLanguage;
  }

  const std::string country_code = GetVariationConfigCountryCode();
  if (country_code != "us" &&
      !base::FeatureList::IsEnabled(
          password_manager::features::kReduceRequirementsForPasswordChange)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_UNSUPPORTED_COUNTRY);
    }
    return PasswordChangeAvailability::kUnsupportedCountryCode;
  }

  const bool has_change_url =
      affiliation_service_->GetChangePasswordURL(url).is_valid();
  base::UmaHistogramBoolean(kHasPasswordChangeUrlHistogram, has_change_url);
  if (logger) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_URL_AVAILABLE,
                       has_change_url);
  }
  return has_change_url ? PasswordChangeAvailability::kAvailable
                        : PasswordChangeAvailability::kNotSupportedSite;
}

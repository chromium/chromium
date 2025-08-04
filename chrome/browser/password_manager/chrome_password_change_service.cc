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
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

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

}  // namespace

ChromePasswordChangeService::ChromePasswordChangeService(
    PrefService* pref_service,
    affiliations::AffiliationService* affiliation_service,
    OptimizationGuideKeyedService* optimization_keyed_service,
    password_manager::PasswordManagerSettingsService* settings_service,
    std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager)
    : pref_service_(pref_service),
      affiliation_service_(affiliation_service),
      optimization_keyed_service_(optimization_keyed_service),
      settings_service_(settings_service),
      feature_manager_(std::move(feature_manager)) {}

ChromePasswordChangeService::~ChromePasswordChangeService() {
  CHECK(password_change_delegates_.empty());
}

bool ChromePasswordChangeService::IsPasswordChangeAvailable() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (HasChangePasswordUrlOverride()) {
    return true;
  }

  // Password generation is disabled.
  if (!feature_manager_->IsGenerationEnabled()) {
    return false;
  }

  // User is not eligible.
  if (!optimization_keyed_service_ ||
      !optimization_keyed_service_->ShouldModelExecutionBeAllowedForUser()) {
    return false;
  }

  // Chrome shouldn't offer to save password. Since during password change a
  // password is saved, it shouldn't be offered.
  if (!settings_service_ ||
      !settings_service_->IsSettingEnabled(
          password_manager::PasswordManagerSetting::kOfferToSavePasswords)) {
    return false;
  }

  // The feature is disabled by enterprise policy.
  constexpr int kPolicyDisabled =
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable);
  if (pref_service_->GetInteger(
          optimization_guide::prefs::
              kAutomatedPasswordChangeEnterprisePolicyAllowed) ==
      kPolicyDisabled) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kImprovedPasswordChangeService);
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
  if (!IsPasswordChangeAvailable()) {
    return false;
  }

  if (GetChangePasswordURLOverride(url).is_valid()) {
    return true;
  }

  if (page_language != autofill::LanguageCode("en") &&
      page_language != autofill::LanguageCode("en-US")) {
    return false;
  }

  if (GetVariationConfigCountryCode() != "us") {
    return false;
  }

  const bool has_change_url =
      affiliation_service_->GetChangePasswordURL(url).is_valid();
  base::UmaHistogramBoolean(kHasPasswordChangeUrlHistogram, has_change_url);
  return has_change_url;
}

bool ChromePasswordChangeService::ShouldShowEntryInSettings() const {
  // The feature becomes enabled when user accepts to change a compromised
  // password.
  if (GetFeatureState(pref_service_) !=
      optimization_guide::prefs::FeatureOptInState::kEnabled) {
    return false;
  }
  return IsPasswordChangeAvailable();
}

void ChromePasswordChangeService::OfferPasswordChangeUi(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  GURL change_pwd_url = GetChangePasswordURLOverride(url);
  if (!change_pwd_url.is_valid()) {
    change_pwd_url = affiliation_service_->GetChangePasswordURL(url);
  }

  CHECK(change_pwd_url.is_valid());

  std::unique_ptr<PasswordChangeDelegate> delegate =
      std::make_unique<PasswordChangeDelegateImpl>(
          std::move(change_pwd_url), username, password,
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

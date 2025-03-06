// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

// Returns whether chrome switch for change password URLs is used.
bool HasChangePasswordUrlOverride() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPasswordChangeUrl);
}

// Return overridden change password URL passed to chrome switch.
GURL GetUrlFromCommandArgs() {
  return GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPasswordChangeUrl));
}

// Returns whether overridden change password URL matches with `url`.
bool IsUrlMatchingOverride(const GURL& url) {
  if (!HasChangePasswordUrlOverride()) {
    return false;
  }

  GURL change_password_url = GetUrlFromCommandArgs();
  if (!url.is_valid() || !change_password_url.is_valid()) {
    return false;
  }

  return affiliations::IsExtendedPublicSuffixDomainMatch(
      url, change_password_url, {});
}

}  // namespace

ChromePasswordChangeService::ChromePasswordChangeService(
    affiliations::AffiliationService* affiliation_service,
    OptimizationGuideKeyedService* optimization_keyed_service,
    std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager)
    : affiliation_service_(affiliation_service),
      optimization_keyed_service_(optimization_keyed_service),
      feature_manager_(std::move(feature_manager)) {}

ChromePasswordChangeService::~ChromePasswordChangeService() {
  CHECK(password_change_delegates_.empty());
}

bool ChromePasswordChangeService::IsPasswordChangeAvailable() {
  if (HasChangePasswordUrlOverride()) {
    return true;
  }

  if (!feature_manager_->IsGenerationEnabled()) {
    return false;
  }

  if (!optimization_keyed_service_) {
    return false;
  }
  if (!optimization_keyed_service_->ShouldModelExecutionBeAllowedForUser()) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kImprovedPasswordChangeService);
}

bool ChromePasswordChangeService::IsPasswordChangeSupported(const GURL& url) {
  if (!IsPasswordChangeAvailable()) {
    return false;
  }

  if (IsUrlMatchingOverride(url)) {
    return true;
  }

  const bool has_change_url =
      affiliation_service_->GetChangePasswordURL(url).is_valid();
  base::UmaHistogramBoolean(kHasPasswordChangeUrlHistogram, has_change_url);
  return has_change_url;
}

void ChromePasswordChangeService::OfferPasswordChangeUi(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
  GURL change_pwd_url = IsUrlMatchingOverride(url)
                            ? GetUrlFromCommandArgs()
                            : affiliation_service_->GetChangePasswordURL(url);
  CHECK(change_pwd_url.is_valid());

  std::unique_ptr<PasswordChangeDelegate> delegate =
      std::make_unique<PasswordChangeDelegateImpl>(
          std::move(change_pwd_url), username, password, web_contents);
  delegate->AddObserver(this);
  password_change_delegates_.push_back(std::move(delegate));

  // Init only after `delegate` was added to the vector, so it can be
  // immediately returned by GetPasswordChangeDelegate() when the flow starts.
  static_cast<PasswordChangeDelegateImpl*>(
      password_change_delegates_.back().get())
      ->OfferPasswordChangeUi();
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

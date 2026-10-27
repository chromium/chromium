// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"

#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_delegate_factory_impl.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_event_controller_factory_desktop.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

namespace enterprise_reporting {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// TODO(b/516458264): Clean up feature and deprecate condition which used it.
BASE_FEATURE(kAlwaysUploadExtensionInfo, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)


#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
bool CanUploadExtensionInfo(Profile* profile) {
  if (base::FeatureList::IsEnabled(kAlwaysUploadExtensionInfo)) {
    return true;
  }

  if (enterprise_util::IsProfileAffiliated(profile)) {
    return true;
  }

  const policy::PolicyMap& policies =
      g_browser_process->policy_service()->GetPolicies(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  return !policies.GetValue(policy::key::kExtensionSettings,
                            base::Value::Type::DICT) &&
         !policies.GetValue(policy::key::kExtensionInstallForcelist,
                            base::Value::Type::LIST) &&
         !policies.GetValue(policy::key::kExtensionInstallSources,
                            base::Value::Type::LIST);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

}  // namespace

CloudProfileReportingService::CloudProfileReportingService(Profile* profile)
    : profile_(profile) {
  Init();
}

CloudProfileReportingService::~CloudProfileReportingService() = default;

void CloudProfileReportingService::CreateReportScheduler() {
  // Always tear down any existing client and schedulers first. If the previous
  // user signed out while `CloudPolicyCore` was offline (`core->client()` was
  // null), `OnCoreDisconnecting()` was skipped and the old user's registered
  // `cloud_policy_client_` is still alive.
  ResetReportScheduler();
  active_dm_token_ = reporting::GetUserDmToken(profile_);

  std::string profile_id = "";
  if (enterprise::ProfileIdServiceFactory::GetForProfile(profile_)) {
    profile_id = enterprise::ProfileIdServiceFactory::GetForProfile(profile_)
                     ->GetProfileId()
                     .value_or("");
  }
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id,
      g_browser_process->browser_policy_connector()
          ->device_management_service(),
      profile_->GetURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

#if BUILDFLAG(IS_ANDROID)
  ReportingDelegateFactoryAndroid delegate_factory;
#else
  ReportingDelegateFactoryDesktop delegate_factory;
#endif  // !BUILDFLAG(IS_ANDROID)
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = delegate_factory.GetReportSchedulerDelegate(profile_);
  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          profile_->GetPath(), &delegate_factory,
          enterprise_signals::SignalsAggregatorFactory::GetForProfile(
              profile_));

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  params.profile_request_generator->ToggleExtensionReport(
      base::BindRepeating(&CanUploadExtensionInfo, profile_));
#endif
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
  if (enterprise_isolated_mode::IsCaaEnabledInIsolatedMode() &&
      profile_->IsEnterpriseIsolatedModeProfile()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kSaasUsageReporting)) {
    auto saas_usage_reporting_delegate_factory =
        SaasUsageReportingDelegateFactoryImpl::CreateForProfile(profile_);
    saas_usage_report_scheduler_ = SaasUsageReportScheduler::Create(
        "profile", saas_usage_reporting_delegate_factory.get());
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // Launch metadata should only be collected and uploaded once per browser
  // session, so do not recreate `browser_launch_controller_` if policy
  // reconnects mid-session.
  if (!browser_launch_controller_ &&
      base::FeatureList::IsEnabled(kBrowserLaunchMetadataReporting)) {
    browser_launch_controller_ =
        BrowserLaunchEventControllerFactoryDesktop::CreateForProfile(profile_);
    if (browser_launch_controller_) {
      browser_launch_controller_->CollectAndUpload();
    }
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
}

void CloudProfileReportingService::Shutdown() {
  core_observation_.Reset();
  browser_launch_controller_.reset();
  ResetReportScheduler();
}

void CloudProfileReportingService::OnCoreConnected(
    policy::CloudPolicyCore* core) {
  // When a managed profile starts up with cached policies on disk, `Init()`'s
  // `BEST_EFFORT` task may have already created `report_scheduler_` for this
  // user before `CloudPolicyCore` finishes connecting online. Skip recreating
  // the scheduler when the DMToken matches to avoid aborting or duplicating
  // startup uploads, while still resetting whenever a different user signs in.
  if (report_scheduler_ && active_dm_token_.has_value() &&
      active_dm_token_ == reporting::GetUserDmToken(profile_)) {
    return;
  }
  CreateReportScheduler();
}

void CloudProfileReportingService::OnRefreshSchedulerStarted(
    policy::CloudPolicyCore* core) {}

void CloudProfileReportingService::OnCoreDisconnecting(
    policy::CloudPolicyCore* core) {
  // Keep `core_observation_` registered so subsequent sign-ins on the same
  // `Profile` (e.g. on Android) trigger `OnCoreConnected()`.
  ResetReportScheduler();
}

void CloudProfileReportingService::OnCoreDestruction(
    policy::CloudPolicyCore* core) {
  core_observation_.Reset();
  ResetReportScheduler();
}

void CloudProfileReportingService::InitForTesting() {
  Init();
}

void CloudProfileReportingService::Init() {
  // Always keep observing `CloudPolicyCore` across the `Profile` lifetime—even
  // if the profile already has cached `CLOUD` management authority at startup—
  // so sign-outs and account switches on single-`Profile` platforms (Android)
  // tear down and recreate the reporting client with the new user's DMToken.
  // Check `IsObserving()` so `InitForTesting()` can safely re-run `Init()`.
  if (!core_observation_.IsObserving() && profile_->GetCloudPolicyManager()) {
    core_observation_.Observe(profile_->GetCloudPolicyManager()->core());
  }

  // Only create ReportScheduler when profile is managed.
  if (policy::ManagementServiceFactory::GetForProfile(profile_)
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD)) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&CloudProfileReportingService::CreateReportScheduler,
                           weak_factory_.GetWeakPtr()));
  }
}

void CloudProfileReportingService::ResetReportScheduler() {
  // Cancel any pending `BEST_EFFORT` `CreateReportScheduler()` task posted by
  // `Init()` so it cannot recreate schedulers after disconnect or overwrite one
  // already created synchronously in `OnCoreConnected()`.
  weak_factory_.InvalidateWeakPtrs();
  active_dm_token_.reset();
  saas_usage_report_scheduler_.reset();
  // Destroy `report_scheduler_` before `cloud_policy_client_` because
  // `ReportScheduler` holds a raw pointer to `cloud_policy_client_`.
  // Intentionally keep `browser_launch_controller_` so launch metadata uploads
  // only once per session.
  report_scheduler_.reset();
  cloud_policy_client_.reset();
}

}  // namespace enterprise_reporting

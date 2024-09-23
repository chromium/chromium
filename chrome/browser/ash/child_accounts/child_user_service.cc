// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/child_accounts/usage_time_limit_processor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/app_constants/constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace ash {

namespace {
// UMA histogram FamilyUser.TimeLimitPolicyTypes
// Reports TimeLimitPolicyType which indicates what time limit policies
// are enabled for current Family Link user. Could report multiple buckets if
// multiple policies are enabled.
constexpr char kTimeLimitPolicyTypesHistogramName[] =
    "FamilyUser.TimeLimitPolicyTypes";

ChildUserService::TimeLimitPolicyType GetTimeLimitPolicyType(
    usage_time_limit::PolicyType policy_type) {
  ChildUserService::TimeLimitPolicyType time_limit_policy;
  switch (policy_type) {
    case usage_time_limit::PolicyType::kFixedLimit:
      time_limit_policy = ChildUserService::TimeLimitPolicyType::kBedTimeLimit;
      break;
    case usage_time_limit::PolicyType::kUsageLimit:
      time_limit_policy =
          ChildUserService::TimeLimitPolicyType::kScreenTimeLimit;
      break;
    case usage_time_limit::PolicyType::kOverride:
      time_limit_policy =
          ChildUserService::TimeLimitPolicyType::kOverrideTimeLimit;
      break;
    case usage_time_limit::PolicyType::kNoPolicy:
      time_limit_policy = ChildUserService::TimeLimitPolicyType::kNoTimeLimit;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return time_limit_policy;
}
}  // namespace

// static
const char ChildUserService::kFamilyLinkHelperAppPackageName[] =
    "com.google.android.apps.kids.familylinkhelper";
// static
const char ChildUserService::kFamilyLinkHelperAppPlayStoreURL[] =
    "https://play.google.com/store/apps/"
    "details?id=com.google.android.apps.kids.familylinkhelper";

ChildUserService::TestApi::TestApi(ChildUserService* service)
    : service_(service) {}

ChildUserService::TestApi::~TestApi() = default;

app_time::AppTimeController* ChildUserService::TestApi::app_time_controller() {
  return service_->app_time_controller_.get();
}

// static
const char* ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest() {
  return kTimeLimitPolicyTypesHistogramName;
}

ChildUserService::ChildUserService(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      app_time_controller_(std::make_unique<app_time::AppTimeController>(
          profile_,
          base::BindRepeating(&ChildUserService::ReportTimeLimitPolicy,
                              base::Unretained(this)))),
      website_approval_notifier_(profile_) {
  DCHECK(profile_);
  app_time_controller_->Init();

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kUsageTimeLimit,
      base::BindRepeating(&ChildUserService::ReportTimeLimitPolicy,
                          base::Unretained(this)));
}

ChildUserService::~ChildUserService() = default;

void ChildUserService::PauseWebActivity(const std::string& app_service_id) {
  NOTIMPLEMENTED();
}

void ChildUserService::ResumeWebActivity(const std::string& app_service_id) {
  NOTIMPLEMENTED();
}

std::optional<base::TimeDelta> ChildUserService::GetTimeLimitForApp(
    const std::string& app_service_id,
    apps::AppType app_type) {
  if (!app_time_controller_)
    return std::nullopt;

  return app_time_controller_->GetTimeLimitForApp(app_service_id, app_type);
}

app_time::AppActivityReportInterface::ReportParams
ChildUserService::GenerateAppActivityReport(
    enterprise_management::ChildStatusReportRequest* report) {
  DCHECK(app_time_controller_);
  app_time_controller_->app_registry()->GenerateHiddenApps(report);
  return app_time_controller_->app_registry()->GenerateAppActivityReport(
      report);
}

void ChildUserService::AppActivityReportSubmitted(
    base::Time report_generation_timestamp) {
  DCHECK(app_time_controller_);
  app_time_controller_->app_registry()->OnSuccessfullyReported(
      report_generation_timestamp);
}

bool ChildUserService::AppTimeLimitAllowlistedApp(
    const app_time::AppId& app_id) const {
  if (!app_time_controller_)
    return false;
  return app_time_controller_->app_registry()->IsAllowlistedApp(app_id);
}

void ChildUserService ::ReportTimeLimitPolicy() const {
  const base::Value::Dict& time_limit_prefs =
      profile_->GetPrefs()->GetDict(prefs::kUsageTimeLimit);

  std::set<usage_time_limit::PolicyType> enabled_policies =
      usage_time_limit::GetEnabledTimeLimitPolicies(time_limit_prefs);

  for (const auto& policy_type : enabled_policies) {
    TimeLimitPolicyType time_limit_policy = GetTimeLimitPolicyType(policy_type);

    // `enabled_policies` does not contains
    // usage_time_limit::PolicyType::kNoPolicy, so `time_limit_policy` never
    // equals to TimeLimitPolicyType::kNoTimeLimit.
    if (time_limit_policy == TimeLimitPolicyType::kNoTimeLimit)
      continue;

    base::UmaHistogramEnumeration(
        /*name=*/kTimeLimitPolicyTypesHistogramName,
        /*sample=*/time_limit_policy);
  }

  bool has_policy_enabled = !enabled_policies.empty();
  DCHECK(app_time_controller_);
  if (app_time_controller_->HasAppTimeLimitRestriction()) {
    base::UmaHistogramEnumeration(
        /*name=*/kTimeLimitPolicyTypesHistogramName,
        /*sample=*/TimeLimitPolicyType::kAppTimeLimit);
    has_policy_enabled = true;
  }

  if (!has_policy_enabled) {
    base::UmaHistogramEnumeration(
        /*name=*/kTimeLimitPolicyTypesHistogramName,
        /*sample=*/TimeLimitPolicyType::kNoTimeLimit);
  }
}

void ChildUserService::Shutdown() {
  if (app_time_controller_) {
    app_time_controller_->app_registry()->SaveAppActivity();
    app_time_controller_->RecordMetricsOnShutdown();
    app_time_controller_.reset();
  }
  pref_change_registrar_.Remove(prefs::kUsageTimeLimit);
}

}  // namespace ash

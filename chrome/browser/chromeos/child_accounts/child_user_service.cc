// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace chromeos {

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

app_time::WebTimeLimitEnforcer* ChildUserService::TestApi::web_time_enforcer() {
  return app_time_controller()
             ? service_->app_time_controller_->web_time_enforcer()
             : nullptr;
}

ChildUserService::ChildUserService(content::BrowserContext* context) {
  DCHECK(context);
  app_time_controller_ = std::make_unique<app_time::AppTimeController>(
      Profile::FromBrowserContext(context));
}

ChildUserService::~ChildUserService() = default;

void ChildUserService::PauseWebActivity(const std::string& app_service_id) {
  DCHECK(app_time_controller_);

  // Pause web activity only if the app is chrome.
  if (app_service_id != extension_misc::kChromeAppId)
    return;

  app_time::WebTimeLimitEnforcer* web_time_enforcer =
      app_time_controller_->web_time_enforcer();
  DCHECK(web_time_enforcer);

  const base::Optional<app_time::AppLimit>& time_limit =
      app_time_controller_->app_registry()->GetWebTimeLimit();
  DCHECK(time_limit.has_value());
  DCHECK_EQ(time_limit->restriction(), app_time::AppRestriction::kTimeLimit);
  DCHECK(time_limit->daily_limit().has_value());

  web_time_enforcer->OnWebTimeLimitReached(time_limit->daily_limit().value());
}

void ChildUserService::ResumeWebActivity(const std::string& app_service_id) {
  DCHECK(app_time_controller_);

  // Only unpause web activity if the app is chrome.
  if (app_service_id != extension_misc::kChromeAppId)
    return;

  app_time::WebTimeLimitEnforcer* web_time_enforcer =
      app_time_controller_->web_time_enforcer();
  DCHECK(web_time_enforcer);

  web_time_enforcer->OnWebTimeLimitEnded();
}

base::Optional<base::TimeDelta> ChildUserService::GetTimeLimitForApp(
    const std::string& app_service_id,
    apps::mojom::AppType app_type) {
  if (!app_time_controller_)
    return base::nullopt;

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

bool ChildUserService::WebTimeLimitReached() const {
  if (!app_time_controller_ || !app_time_controller_->web_time_enforcer())
    return false;
  return app_time_controller_->web_time_enforcer()->blocked();
}

bool ChildUserService::WebTimeLimitAllowlistedURL(const GURL& url) const {
  if (!app_time_controller_)
    return false;
  DCHECK(app_time_controller_->web_time_enforcer());
  return app_time_controller_->web_time_enforcer()->IsURLAllowlisted(url);
}

bool ChildUserService::AppTimeLimitAllowlistedApp(
    const app_time::AppId& app_id) const {
  if (!app_time_controller_)
    return false;
  return app_time_controller_->app_registry()->IsAllowlistedApp(app_id);
}

base::TimeDelta ChildUserService::GetWebTimeLimit() const {
  DCHECK(app_time_controller_);
  DCHECK(app_time_controller_->web_time_enforcer());
  return app_time_controller_->web_time_enforcer()->time_limit();
}

void ChildUserService::GetEnabledAppTimeLimitPolicies(
    std::set<FamilyUserParentalControlMetrics::TimeLimitPolicyType>*
        enabled_policies) {
  if (!app_time_controller_ || !enabled_policies)
    return;

  if (app_time_controller_->HasWebTimeLimitRestriction()) {
    enabled_policies->insert(
        FamilyUserParentalControlMetrics::TimeLimitPolicyType::kWebTimeLimit);
  }

  if (app_time_controller_->HasAppTimeLimitRestriction()) {
    enabled_policies->insert(
        FamilyUserParentalControlMetrics::TimeLimitPolicyType::kAppTimeLimit);
  }
}

void ChildUserService::Shutdown() {
  if (app_time_controller_) {
    app_time_controller_->app_registry()->SaveAppActivity();
    app_time_controller_->RecordMetricsOnShutdown();
    app_time_controller_.reset();
  }
}

}  // namespace chromeos

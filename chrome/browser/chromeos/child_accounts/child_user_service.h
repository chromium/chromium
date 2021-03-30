// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/chromeos/child_accounts/family_user_parental_control_metrics.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_report_interface.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_interface.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_management {
class ChildStatusReportRequest;
}  // namespace enterprise_management

class GURL;

namespace chromeos {
namespace app_time {
class AppId;
class AppTimeController;
class WebTimeLimitEnforcer;
}  // namespace app_time

// Facade that exposes child user related functionality on Chrome OS.
// TODO(crbug.com/1022231): Migrate ConsumerStatusReportingService,
// EventBasedStatusReporting and ScreenTimeController to ChildUserService.
class ChildUserService : public KeyedService,
                         public app_time::AppTimeLimitInterface,
                         public app_time::AppActivityReportInterface {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(ChildUserService* service);
    ~TestApi();

    app_time::WebTimeLimitEnforcer* web_time_enforcer();
    app_time::AppTimeController* app_time_controller();

   private:
    ChildUserService* const service_;
  };

  // Family Link helper(for child and teens) is an app available to supervised
  // users and the companion app of Family Link app(for parents).
  static const char kFamilyLinkHelperAppPackageName[];
  static const char kFamilyLinkHelperAppPlayStoreURL[];

  explicit ChildUserService(content::BrowserContext* context);
  ChildUserService(const ChildUserService&) = delete;
  ChildUserService& operator=(const ChildUserService&) = delete;
  ~ChildUserService() override;

  // app_time::AppTimeLimitInterface:
  void PauseWebActivity(const std::string& app_service_id) override;
  void ResumeWebActivity(const std::string& app_service_id) override;
  base::Optional<base::TimeDelta> GetTimeLimitForApp(
      const std::string& app_service_id,
      apps::mojom::AppType app_type) override;

  // app_time::AppActivityReportInterface:
  app_time::AppActivityReportInterface::ReportParams GenerateAppActivityReport(
      enterprise_management::ChildStatusReportRequest* report) override;
  void AppActivityReportSubmitted(
      base::Time report_generation_timestamp) override;

  // Returns whether web time limit was reached for child user.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitReached() const;

  // Returns whether given |url| can be used without any time restrictions.
  // Viewing of allowlisted |url| does not count towards usage web time.
  // Always returns false if per-app times limits feature is disabled.
  bool WebTimeLimitAllowlistedURL(const GURL& url) const;

  // Returns whether the application with id |app_id| can be used without any
  // time restrictions.
  bool AppTimeLimitAllowlistedApp(const app_time::AppId& app_id) const;

  // Returns time limit set for using the web on a given day.
  // Should only be called if |features::kPerAppTimeLimits| and
  // |features::kWebTimeLimits| features are enabled.
  base::TimeDelta GetWebTimeLimit() const;

  // Checks whether app time limit and chrome app time limit are enabled and
  // inserts the enabled types to `enabled_policies`.
  void GetEnabledAppTimeLimitPolicies(
      std::set<FamilyUserParentalControlMetrics::TimeLimitPolicyType>*
          enabled_policies);

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<app_time::AppTimeController> app_time_controller_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_USER_SERVICE_H_

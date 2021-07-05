// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_

#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/util/status.h"

class Profile;

namespace net {
class BackoffEntry;
}  // namespace net

namespace reporting {
class ReportQueue;
}  // namespace reporting

namespace policy {

// Initializes an instance of DlpRulesManager when a primary managed profile is
// being created, e.g. when managed user signs in.
class DlpRulesManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>;

  static DlpRulesManagerFactory* GetInstance();
  // Returns nullptr if there is no primary profile, e.g. the session is not
  // started.
  static DlpRulesManager* GetForPrimaryProfile();

 private:
  friend class base::NoDestructor<DlpRulesManagerFactory>;

  DlpRulesManagerFactory();
  ~DlpRulesManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // TODO(1198500, marcgrimme, refactor into it's own class and share code
  // with PrintJobReportingServiceFactory.
  static void BuildReportingQueue(Profile* profile, SuccessCallback success_cb);
  static void TrySetReportQueue(
      SuccessCallback success_cb,
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
          report_queue_result);
  static reporting::ReportQueueProvider::CreateReportQueueCallback
  CreateTrySetCallback(policy::DMToken dm_token,
                       SuccessCallback success_cb,
                       std::unique_ptr<net::BackoffEntry> backoff_entry);
};

}  // namespace policy
#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_

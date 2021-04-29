// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"

#include "base/bind_post_task.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/backoff_entry.h"

namespace policy {

// static
DlpRulesManagerFactory* DlpRulesManagerFactory::GetInstance() {
  static base::NoDestructor<DlpRulesManagerFactory> factory;
  return factory.get();
}

// static
DlpRulesManager* DlpRulesManagerFactory::GetForPrimaryProfile() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile)
    return nullptr;
  return static_cast<DlpRulesManager*>(
      DlpRulesManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

DlpRulesManagerFactory::DlpRulesManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DlpRulesManager",
          BrowserContextDependencyManager::GetInstance()) {}

bool DlpRulesManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // We have to create the instance immediately because it's responsible for
  // instantiation of DataTransferDlpController. Otherwise even if the policy is
  // present, DataTransferDlpController won't be instantiated and therefore no
  // policy will be applied.
  return true;
}

KeyedService* DlpRulesManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // UserManager might be not available in tests.
  if (!user_manager::UserManager::IsInitialized() || !profile ||
      !chromeos::ProfileHelper::IsPrimaryProfile(profile) ||
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  PrefService* local_state = g_browser_process->local_state();
  // Might be not available in tests.
  if (!local_state)
    return nullptr;

  DlpRulesManagerImpl* manager = new DlpRulesManagerImpl(local_state);
  if (manager->reporting_manager_)
    BuildReportingQueue(profile,
                        manager->reporting_manager_->GetReportQueueSetter());
  return manager;
}

// static
void DlpRulesManagerFactory::BuildReportingQueue(Profile* profile,
                                                 SuccessCallback success_cb) {
  auto dm_token = GetDMToken(profile, /*only_affiliated=*/false);
  if (!dm_token.is_valid()) {
    VLOG(1) << "DlpReporting has invalid DMToken. Reporting disabled.";
    return;
  }

  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token.value(), reporting::Destination::DLP_EVENTS,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));
  if (!config_result.ok()) {
    VLOG(1) << "ReportQueueConfiguration must be valid";
    return;
  }

  // Asynchronously create and try to set ReportQueue.
  auto try_set_cb = CreateTrySetCallback(dm_token, std::move(success_cb),
                                         reporting::GetBackoffEntry());
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(reporting::ReportQueueProvider::CreateQueue,
                                std::move(config_result.ValueOrDie()),
                                std::move(try_set_cb)));
}

// static
void DlpRulesManagerFactory::TrySetReportQueue(
    SuccessCallback success_cb,
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    VLOG(1) << "ReportQueue could not be created";
    return;
  }
  std::move(success_cb).Run(std::move(report_queue_result.ValueOrDie()));
}

// static
reporting::ReportQueueProvider::CreateReportQueueCallback
DlpRulesManagerFactory::CreateTrySetCallback(
    policy::DMToken dm_token,
    SuccessCallback success_cb,
    std::unique_ptr<net::BackoffEntry> backoff_entry) {
  return base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&DlpRulesManagerFactory::TrySetReportQueue,
                     std::move(success_cb)));
}

}  // namespace policy

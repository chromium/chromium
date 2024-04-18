// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "device_management_backend.pb.h"

namespace policy {

BASE_FEATURE(kUseEncryptedReportingPipelineToReportArcAppInstallEvents,
             "UseEncryptedReportingPipelineToReportArcAppInstallEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

AppInstallEventLogManagerWrapper::~AppInstallEventLogManagerWrapper() = default;

// static
AppInstallEventLogManagerWrapper*
AppInstallEventLogManagerWrapper::CreateForProfile(Profile* profile) {
  AppInstallEventLogManagerWrapper* wrapper =
      new AppInstallEventLogManagerWrapper(profile);
  wrapper->Init();
  return wrapper;
}

// static
void AppInstallEventLogManagerWrapper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kArcAppInstallEventLoggingEnabled,
                                false);
}

AppInstallEventLogManagerWrapper::AppInstallEventLogManagerWrapper(
    Profile* profile)
    : use_encrypted_reporting_pipeline_(base::FeatureList::IsEnabled(
          kUseEncryptedReportingPipelineToReportArcAppInstallEvents)),
      profile_(profile) {
  log_task_runner_ =
      std::make_unique<ArcAppInstallEventLogManager::LogTaskRunnerWrapper>();

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcAppInstallEventLoggingEnabled,
      base::BindRepeating(&AppInstallEventLogManagerWrapper::EvaluatePref,
                          base::Unretained(this)));
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&AppInstallEventLogManagerWrapper::OnAppTerminating,
                         base::Unretained(this)));
}

void AppInstallEventLogManagerWrapper::Init() {
  EvaluatePref();
}

void AppInstallEventLogManagerWrapper::CreateManager() {
  log_manager_ = std::make_unique<ArcAppInstallEventLogManager>(
      log_task_runner_.get(),
      profile_->GetUserCloudPolicyManagerAsh()->GetAppInstallEventLogUploader(),
      profile_);
}

void AppInstallEventLogManagerWrapper::DestroyManager() {
  log_manager_.reset();
}

void AppInstallEventLogManagerWrapper::CreateEncryptedReporter() {
  // Log events using the encrypted reporting pipeline.
  ::reporting::SourceInfo source_info;
  source_info.set_source(::reporting::SourceInfo::ASH);
  auto report_queue =
      ::reporting::ReportQueueFactory::CreateSpeculativeReportQueue(
          ::reporting::ReportQueueConfiguration::Create(
              {.event_type = ::reporting::EventType::kUser,
               .destination = ::reporting::Destination::ARC_INSTALL})
              .SetSourceInfo(std::move(source_info)));
  encrypted_reporter_ = std::make_unique<ArcAppInstallEncryptedEventReporter>(
      std::move(report_queue), profile_);
}

void AppInstallEventLogManagerWrapper::DestroyEncryptedReporter() {
  encrypted_reporter_.reset();
}

void AppInstallEventLogManagerWrapper::InitLogging() {
  if (use_encrypted_reporting_pipeline_) {
    CreateEncryptedReporter();
  } else if (!log_manager_) {
    // Log events using the cloud policy client.
    CreateManager();
  }
}

void AppInstallEventLogManagerWrapper::DisableLogging() {
  if (use_encrypted_reporting_pipeline_) {
    DestroyEncryptedReporter();
  } else {
    DestroyManager();
  }
}

void AppInstallEventLogManagerWrapper::EvaluatePref() {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kArcAppInstallEventLoggingEnabled)) {
    InitLogging();
  } else {
    DisableLogging();
    ArcAppInstallEventLogManager::Clear(log_task_runner_.get(), profile_);
  }
}

void AppInstallEventLogManagerWrapper::OnAppTerminating() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace policy

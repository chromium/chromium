// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"

#include <memory>

#include "ash/constants/ash_policy_pref_names.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "device_management_backend.pb.h"

namespace policy {

AppInstallEventLogManagerWrapper::~AppInstallEventLogManagerWrapper() = default;

// static
void AppInstallEventLogManagerWrapper::CreateForProfile(
    PrefService* local_state,
    Profile* profile) {
  // `wrapper` manages its own lifetime.
  AppInstallEventLogManagerWrapper* wrapper =
      new AppInstallEventLogManagerWrapper(local_state, profile);
  wrapper->Init();
}

// static
void AppInstallEventLogManagerWrapper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                false);
}

AppInstallEventLogManagerWrapper::AppInstallEventLogManagerWrapper(
    PrefService* local_state,
    Profile* profile)
    : local_state_(CHECK_DEREF(local_state)), profile_(profile) {
  session_termination_observation_.Observe(
      ash::SessionTerminationManager::Get());

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      ash::prefs::kArcAppInstallEventLoggingEnabled,
      base::BindRepeating(&AppInstallEventLogManagerWrapper::EvaluatePref,
                          base::Unretained(this)));
}

void AppInstallEventLogManagerWrapper::Init() {
  EvaluatePref();
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
      &local_state_.get(), std::move(report_queue), profile_);
}

void AppInstallEventLogManagerWrapper::DestroyEncryptedReporter() {
  encrypted_reporter_.reset();
}

void AppInstallEventLogManagerWrapper::InitLogging() {
  CreateEncryptedReporter();
}

void AppInstallEventLogManagerWrapper::DisableLogging() {
  DestroyEncryptedReporter();
}

void AppInstallEventLogManagerWrapper::EvaluatePref() {
  if (profile_->GetPrefs()->GetBoolean(
          ash::prefs::kArcAppInstallEventLoggingEnabled)) {
    InitLogging();
  } else {
    DisableLogging();
    ArcAppInstallEventLogger::Clear(profile_);
  }
}

void AppInstallEventLogManagerWrapper::OnAppTerminating() {
  session_termination_observation_.Reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace policy

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/util/status.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace policy {
// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Mode DlpRulesManagerLevel2DlpEventMode(
    DlpRulesManager::Level level) {
  switch (level) {
    case DlpRulesManager::Level::kBlock:
      return DlpPolicyEvent_Mode_BLOCK;
    case DlpRulesManager::Level::kWarn:
      return DlpPolicyEvent_Mode_BLOCK;
    case DlpRulesManager::Level::kNotSet:
      return DlpPolicyEvent_Mode_UNDEFINED_MODE;
    default:
      return DlpPolicyEvent_Mode_UNDEFINED_MODE;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Restriction DlpRulesManagerRestriction2DlpEventRestriction(
    DlpRulesManager::Restriction restriction) {
  switch (restriction) {
    case DlpRulesManager::Restriction::kPrinting:
      return DlpPolicyEvent_Restriction_PRINTING;
    case DlpRulesManager::Restriction::kScreenshot:
      return DlpPolicyEvent_Restriction_SCREENSHOT;
    case DlpRulesManager::Restriction::kScreenShare:
      return DlpPolicyEvent_Restriction_SCREENCAST;
    case DlpRulesManager::Restriction::kPrivacyScreen:
      return DlpPolicyEvent_Restriction_EPRIVACY;
    case DlpRulesManager::Restriction::kClipboard:
      return DlpPolicyEvent_Restriction_CLIPBOARD;
    default:
      return DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION;
  }
}

DlpPolicyEvent* CreateDlpPolicyEvent(content::WebContents* source,
                                     DlpRulesManager::Level level,
                                     DlpRulesManager::Restriction restriction) {
  DlpPolicyEvent* event = new DlpPolicyEvent();

  DlpPolicyEventSource* event_source = new DlpPolicyEventSource();
  event_source->set_url(source->GetURL().spec());
  event->set_allocated_source(event_source);

  // TODO(1187479, marcgrimme): add proper destination as soon as available
  // DlpPolicyEventDestination* event_destination = new
  // DlpPolicyEventDestination();
  // event_destination->set_component(DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT);
  // event->set_allocated_destination(event_destination);

  event->set_restriction(
      DlpRulesManagerRestriction2DlpEventRestriction(restriction));
  event->set_mode(DlpRulesManagerLevel2DlpEventMode(level));
  event->set_timestamp(base::Time::Now().ToTimeT());

  return event;
}

static DlpReportingManager* g_dlp_reporting_manager = nullptr;

// static
void DlpReportingManager::Init() {
  if (g_dlp_reporting_manager)
    return;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;
  auto dm_token = GetDMToken(profile, /*only_affiliated=*/false);
  if (!dm_token.is_valid()) {
    LOG(ERROR) << "DlpReporting has invalid DMToken. Reporting disabled.";
    return;
  }

  g_dlp_reporting_manager = new DlpReportingManager();
  g_dlp_reporting_manager->BuildReportQueueConfiguration(
      dm_token,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));
  g_dlp_reporting_manager->BuildReportQueue(
      base::BindOnce(&DlpReportingManager::OnReportQueueResult,
                     base::Unretained(g_dlp_reporting_manager)));
}

// static
DlpReportingManager* DlpReportingManager::Get() {
  DlpReportingManager::Init();
  return g_dlp_reporting_manager;
}

// static
void DlpReportingManager::SetDlpReportingManagerForTesting(
    DlpReportingManager* manager) {
  if (g_dlp_reporting_manager) {
    delete g_dlp_reporting_manager;
  }
  g_dlp_reporting_manager = manager;
}

DlpReportingManager::DlpReportingManager() = default;
DlpReportingManager::~DlpReportingManager() = default;

void DlpReportingManager::BuildReportQueueConfiguration(
    const policy::DMToken& dm_token,
    reporting::ReportQueueConfiguration::PolicyCheckCallback callback) {
  report_queue_config_ = reporting::ReportQueueConfiguration::Create(
      dm_token.value(), reporting::Destination::DLP_EVENTS, callback);
}

void DlpReportingManager::BuildReportQueue(
    reporting::ReportingClient::CreateReportQueueCallback callback) {
  if (report_queue_config_.ok()) {
    ::reporting::ReportQueueProvider::CreateQueue(
        std::move(report_queue_config_.ValueOrDie()), std::move(callback));
  }
}

void DlpReportingManager::OnReportQueueResult(
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    LOG(ERROR) << "Report queue could not be setup because of "
               << report_queue_result.status();
    return;
  }
  report_queue_ = std::move(report_queue_result.ValueOrDie());
}

void DlpReportingManager::ReportPrintingEvent(
    content::WebContents* web_contents,
    DlpRulesManager::Level level) const {
  // TODO(1187506, marcgrimme) Refactor to handle gracefully with user
  // interaction when queue is not ready.
  if (!report_queue_) {
    LOG(ERROR) << "Report queue could not be initialized. DLP reporting "
                  "functionality will be disabled.";
    return;
  }
  if (!ReportingEnabled()) {
    LOG(ERROR) << "Reporting functionality for DLP is explicitly disabled.";
    return;
  }
  reporting::ReportQueue::EnqueueCallback callback = base::BindOnce(
      &DlpReportingManager::OnEventEnqueued, base::Unretained(this));
  report_queue_->Enqueue(
      CreateDlpPolicyEvent(web_contents, level,
                           DlpRulesManager::Restriction::kPrinting),
      reporting::Priority::IMMEDIATE, std::move(callback));
}

void DlpReportingManager::OnEventEnqueued(reporting::Status status) const {
  if (!status.ok()) {
    LOG(ERROR) << "Could not enqueue event to DLP reporting queue because of "
               << status;
  }
}
}  // namespace policy

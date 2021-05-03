// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"
#include "content/public/browser/browser_thread.h"
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

DlpPolicyEvent* CreateDlpPolicyEvent(const std::string& src_pattern,
                                     DlpRulesManager::Level level,
                                     DlpRulesManager::Restriction restriction) {
  DlpPolicyEvent* event = new DlpPolicyEvent();

  DlpPolicyEventSource* event_source = new DlpPolicyEventSource();
  event_source->set_url(src_pattern);
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

DlpReportingManager::DlpReportingManager() : report_queue_{nullptr} {}
DlpReportingManager::~DlpReportingManager() = default;

DlpReportingManager::ReportQueueSetterCallback
DlpReportingManager::GetReportQueueSetter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&DlpReportingManager::SetReportQueue,
                        weak_factory_.GetWeakPtr());
}

void DlpReportingManager::SetReportQueue(
    std::unique_ptr<reporting::ReportQueue> report_queue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  report_queue_ = std::move(report_queue);
}

void DlpReportingManager::ReportPrintingEvent(
    const std::string& src_pattern,
    DlpRulesManager::Level level) const {
  // TODO(1187506, marcgrimme) Refactor to handle gracefully with user
  // interaction when queue is not ready.
  if (!report_queue_.get()) {
    DLOG(WARNING) << "Report queue could not be initialized. DLP reporting "
                     "functionality will be disabled.";
    return;
  }
  reporting::ReportQueue::EnqueueCallback callback = base::BindOnce(
      &DlpReportingManager::OnEventEnqueued, base::Unretained(this));
  report_queue_->Enqueue(
      CreateDlpPolicyEvent(src_pattern, level,
                           DlpRulesManager::Restriction::kPrinting),
      reporting::Priority::IMMEDIATE, std::move(callback));
}

void DlpReportingManager::OnEventEnqueued(reporting::Status status) const {
  if (!status.ok()) {
    VLOG(1) << "Could not enqueue event to DLP reporting queue because of "
            << status;
  }
}
}  // namespace policy

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace policy {
// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Mode DlpRulesManagerLevel2DlpEventMode(
    DlpRulesManager::Level level) {
  switch (level) {
    case DlpRulesManager::Level::kBlock:
      return DlpPolicyEvent_Mode_BLOCK;
    case DlpRulesManager::Level::kWarn:
      return DlpPolicyEvent_Mode_WARN;
    case DlpRulesManager::Level::kReport:
      return DlpPolicyEvent_Mode_REPORT;
    case DlpRulesManager::Level::kNotSet:
    case DlpRulesManager::Level::kAllow:
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
    case DlpRulesManager::Restriction::kFiles:
      return DlpPolicyEvent_Restriction_FILES;
    case DlpRulesManager::Restriction::kUnknownRestriction:
      return DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpRulesManager::Restriction DlpEventRestriction2DlpRulesManagerRestriction(
    DlpPolicyEvent_Restriction restriction) {
  switch (restriction) {
    case DlpPolicyEvent_Restriction_PRINTING:
      return DlpRulesManager::Restriction::kPrinting;
    case DlpPolicyEvent_Restriction_SCREENSHOT:
      return DlpRulesManager::Restriction::kScreenshot;
    case DlpPolicyEvent_Restriction_SCREENCAST:
      return DlpRulesManager::Restriction::kScreenShare;
    case DlpPolicyEvent_Restriction_EPRIVACY:
      return DlpRulesManager::Restriction::kPrivacyScreen;
    case DlpPolicyEvent_Restriction_CLIPBOARD:
      return DlpRulesManager::Restriction::kClipboard;
    case DlpPolicyEvent_Restriction_FILES:
      return DlpRulesManager::Restriction::kFiles;
    case DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION:
      return DlpRulesManager::Restriction::kUnknownRestriction;
  }
}

DlpPolicyEvent_UserType GetCurrentUserType() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Could be not initialized in tests.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->GetPrimaryUser()) {
    return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
  }
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return DlpPolicyEvent_UserType_REGULAR;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return DlpPolicyEvent_UserType_MANAGED_GUEST;
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return DlpPolicyEvent_UserType_KIOSK;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_CHILD:
      return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
    case user_manager::NUM_USER_TYPES:
      NOTREACHED();
      return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  switch (chromeos::BrowserParamsProxy::Get()->SessionType()) {
    case crosapi::mojom::SessionType::kRegularSession:
      return DlpPolicyEvent_UserType_REGULAR;
    case crosapi::mojom::SessionType::kPublicSession:
      return DlpPolicyEvent_UserType_MANAGED_GUEST;
    case crosapi::mojom::SessionType::kWebKioskSession:
    case crosapi::mojom::SessionType::kAppKioskSession:
      return DlpPolicyEvent_UserType_KIOSK;
    case crosapi::mojom::SessionType::kUnknown:
    case crosapi::mojom::SessionType::kGuestSession:
    case crosapi::mojom::SessionType::kChildSession:
      return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// static
std::unique_ptr<DlpPolicyEventBuilder> DlpPolicyEventBuilder::Event(
    const std::string& src_pattern,
    const std::string& rule_name,
    const std::string& rule_id,
    DlpRulesManager::Restriction restriction,
    DlpRulesManager::Level level) {
  std::unique_ptr<DlpPolicyEventBuilder> event_builder(
      new DlpPolicyEventBuilder());
  event_builder->SetSourcePattern(src_pattern);
  event_builder->SetRestriction(restriction);
  event_builder->event.set_mode(DlpRulesManagerLevel2DlpEventMode(level));

  if (!rule_name.empty()) {
    event_builder->event.set_triggered_rule_name(rule_name);
  }
  if (!rule_id.empty()) {
    event_builder->event.set_triggered_rule_id(rule_id);
  }
  return event_builder;
}

// static
std::unique_ptr<DlpPolicyEventBuilder>
DlpPolicyEventBuilder::WarningProceededEvent(
    const std::string& src_pattern,
    const std::string& rule_name,
    const std::string& rule_id,
    DlpRulesManager::Restriction restriction) {
  std::unique_ptr<DlpPolicyEventBuilder> event_builder(
      new DlpPolicyEventBuilder());
  event_builder->SetSourcePattern(src_pattern);
  event_builder->SetRestriction(restriction);
  event_builder->event.set_mode(DlpPolicyEvent_Mode_WARN_PROCEED);
  return event_builder;
}

DlpPolicyEventBuilder::DlpPolicyEventBuilder() {
  int64_t timestamp_micro =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  event.set_timestamp_micro(timestamp_micro);
  event.set_user_type(GetCurrentUserType());
}

void DlpPolicyEventBuilder::SetDestinationPattern(
    const std::string& dst_pattern) {
  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  event_destination->set_url(dst_pattern);
  event.set_allocated_destination(event_destination);
}

void DlpPolicyEventBuilder::SetDestinationComponent(
    data_controls::Component dst_component) {
  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  switch (dst_component) {
    case (data_controls::Component::kArc):
      event_destination->set_component(DlpPolicyEventDestination_Component_ARC);
      break;
    case (data_controls::Component::kCrostini):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_CROSTINI);
      break;
    case (data_controls::Component::kPluginVm):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_PLUGIN_VM);
      break;
    case (data_controls::Component::kUsb):
      event_destination->set_component(DlpPolicyEventDestination_Component_USB);
      break;
    case (data_controls::Component::kDrive):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_DRIVE);
      break;
    case (data_controls::Component::kOneDrive):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_ONEDRIVE);
      break;
    case (data_controls::Component::kUnknownComponent):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT);
      break;
  }
  event.set_allocated_destination(event_destination);
}

void DlpPolicyEventBuilder::SetContentName(const std::string& content_name) {
  event.set_content_name(content_name);
}

DlpPolicyEvent DlpPolicyEventBuilder::Create() {
  return event;
}

void DlpPolicyEventBuilder::SetSourcePattern(const std::string& src_pattern) {
  DlpPolicyEventSource* event_source = new DlpPolicyEventSource;
  event_source->set_url(src_pattern);
  event.set_allocated_source(event_source);
}

void DlpPolicyEventBuilder::SetRestriction(
    DlpRulesManager::Restriction restriction) {
  event.set_restriction(
      DlpRulesManagerRestriction2DlpEventRestriction(restriction));
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    DlpRulesManager::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(
      src_pattern, rule_name, rule_id, restriction, level);
  return event_builder->Create();
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    const std::string& dst_pattern,
                                    DlpRulesManager::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    DlpRulesManager::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(
      src_pattern, rule_name, rule_id, restriction, level);
  event_builder->SetDestinationPattern(dst_pattern);
  return event_builder->Create();
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_pattern,
                                    data_controls::Component dst_component,
                                    DlpRulesManager::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    DlpRulesManager::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(
      src_pattern, rule_name, rule_id, restriction, level);
  event_builder->SetDestinationComponent(dst_component);

  return event_builder->Create();
}

DlpReportingManager::DlpReportingManager()
    : report_queue_(
          ::reporting::ReportQueueFactory::CreateSpeculativeReportQueue([]() {
            ::reporting::SourceInfo source_info;
            source_info.set_source(::reporting::SourceInfo::ASH);
            return ::reporting::ReportQueueConfiguration::Create(
                       {.event_type = ::reporting::EventType::kUser,
                        .destination = ::reporting::Destination::DLP_EVENTS})
                .SetSourceInfo(std::move(source_info));
          }())) {}

DlpReportingManager::~DlpReportingManager() = default;

void DlpReportingManager::SetReportQueueForTest(
    std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue) {
  report_queue_.reset();
  report_queue_ = std::move(report_queue);
}

void DlpReportingManager::ReportEvent(const std::string& src_pattern,
                                      DlpRulesManager::Restriction restriction,
                                      DlpRulesManager::Level level,
                                      const std::string& rule_name,
                                      const std::string& rule_id) {
  auto event =
      CreateDlpPolicyEvent(src_pattern, restriction, rule_name, rule_id, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::ReportEvent(const std::string& src_pattern,
                                      const std::string& dst_pattern,
                                      DlpRulesManager::Restriction restriction,
                                      DlpRulesManager::Level level,
                                      const std::string& rule_name,
                                      const std::string& rule_id) {
  auto event = CreateDlpPolicyEvent(src_pattern, dst_pattern, restriction,
                                    rule_name, rule_id, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::ReportEvent(
    const std::string& src_pattern,
    const data_controls::Component dst_component,
    DlpRulesManager::Restriction restriction,
    DlpRulesManager::Level level,
    const std::string& rule_name,
    const std::string& rule_id) {
  auto event = CreateDlpPolicyEvent(src_pattern, dst_component, restriction,
                                    rule_name, rule_id, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::OnEventEnqueued(reporting::Status status) {
  if (!status.ok()) {
    VLOG(1) << "Could not enqueue event to DLP reporting queue because of "
            << status;
  }
  events_reported_++;
  base::UmaHistogramEnumeration(
      GetDlpHistogramPrefix() + dlp::kReportedEventStatus, status.code(),
      reporting::error::Code::MAX_VALUE);
}

void DlpReportingManager::ReportEvent(DlpPolicyEvent event) {
  // TODO(1187506, marcgrimme) Refactor to handle gracefully with user
  // interaction when queue is not ready.
  DlpBooleanHistogram(dlp::kErrorsReportQueueNotReady, !report_queue_.get());
  if (!report_queue_.get()) {
    DLOG(WARNING) << "Report queue could not be initialized. DLP reporting "
                     "functionality will be disabled.";
    return;
  }
  reporting::ReportQueue::EnqueueCallback callback = base::BindOnce(
      &DlpReportingManager::OnEventEnqueued, weak_factory_.GetWeakPtr());

  switch (event.mode()) {
    case DlpPolicyEvent_Mode_BLOCK:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedBlockLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_REPORT:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedReportLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_WARN:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedWarnLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_WARN_PROCEED:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() + dlp::kReportedWarnProceedLevelRestriction,
          DlpEventRestriction2DlpRulesManagerRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_UNDEFINED_MODE:
      NOTREACHED();
      break;
  }
  report_queue_->Enqueue(std::make_unique<DlpPolicyEvent>(std::move(event)),
                         reporting::Priority::SLOW_BATCH, std::move(callback));
  VLOG(1) << "DLP event sent to reporting infrastructure.";
}

}  // namespace policy

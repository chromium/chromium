// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "content/public/common/content_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace data_controls {
// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Mode RuleLevel2DlpEventMode(
    Rule::Level level) {
  switch (level) {
    case Rule::Level::kBlock:
      return DlpPolicyEvent_Mode_BLOCK;
    case Rule::Level::kWarn:
      return DlpPolicyEvent_Mode_WARN;
    case Rule::Level::kReport:
      return DlpPolicyEvent_Mode_REPORT;
    case Rule::Level::kNotSet:
    case Rule::Level::kAllow:
      return DlpPolicyEvent_Mode_UNDEFINED_MODE;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
DlpPolicyEvent_Restriction RuleRestriction2DlpEventRestriction(
    Rule::Restriction restriction) {
  switch (restriction) {
    case Rule::Restriction::kPrinting:
      return DlpPolicyEvent_Restriction_PRINTING;
    case Rule::Restriction::kScreenshot:
      return DlpPolicyEvent_Restriction_SCREENSHOT;
    case Rule::Restriction::kScreenShare:
      return DlpPolicyEvent_Restriction_SCREENCAST;
    case Rule::Restriction::kPrivacyScreen:
      return DlpPolicyEvent_Restriction_EPRIVACY;
    case Rule::Restriction::kClipboard:
      return DlpPolicyEvent_Restriction_CLIPBOARD;
    case Rule::Restriction::kFiles:
      return DlpPolicyEvent_Restriction_FILES;
    case Rule::Restriction::kUnknownRestriction:
      return DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION;
  }
}

// TODO(1187477, marcgrimme): revisit if this should be refactored.
Rule::Restriction DlpEventRestriction2RuleRestriction(
    DlpPolicyEvent_Restriction restriction) {
  switch (restriction) {
    case DlpPolicyEvent_Restriction_PRINTING:
      return Rule::Restriction::kPrinting;
    case DlpPolicyEvent_Restriction_SCREENSHOT:
      return Rule::Restriction::kScreenshot;
    case DlpPolicyEvent_Restriction_SCREENCAST:
      return Rule::Restriction::kScreenShare;
    case DlpPolicyEvent_Restriction_EPRIVACY:
      return Rule::Restriction::kPrivacyScreen;
    case DlpPolicyEvent_Restriction_CLIPBOARD:
      return Rule::Restriction::kClipboard;
    case DlpPolicyEvent_Restriction_FILES:
      return Rule::Restriction::kFiles;
    case DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION:
      return Rule::Restriction::kUnknownRestriction;
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
    case user_manager::UserType::kRegular:
      return DlpPolicyEvent_UserType_REGULAR;
    case user_manager::UserType::kPublicAccount:
      return DlpPolicyEvent_UserType_MANAGED_GUEST;
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return DlpPolicyEvent_UserType_KIOSK;
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kChild:
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
#else
  // TODO(b/303640183): Revisit what this should return for non-CrOS platforms.
  return DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// static
std::unique_ptr<DlpPolicyEventBuilder> DlpPolicyEventBuilder::Event(
    const std::string& src_url,
    const std::string& rule_name,
    const std::string& rule_id,
    Rule::Restriction restriction,
    Rule::Level level) {
  std::unique_ptr<DlpPolicyEventBuilder> event_builder(
      new DlpPolicyEventBuilder());
  event_builder->SetSourceUrl(src_url);
  event_builder->SetRestriction(restriction);
  event_builder->event.set_mode(RuleLevel2DlpEventMode(level));

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
DlpPolicyEventBuilder::WarningProceededEvent(const std::string& src_url,
                                             const std::string& rule_name,
                                             const std::string& rule_id,
                                             Rule::Restriction restriction) {
  std::unique_ptr<DlpPolicyEventBuilder> event_builder(
      new DlpPolicyEventBuilder());
  event_builder->SetSourceUrl(src_url);
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

void DlpPolicyEventBuilder::SetDestinationUrl(const std::string& dst_url) {
  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  event_destination->set_url(dst_url.substr(0, content::kMaxURLDisplayChars));
  event.set_allocated_destination(event_destination);
}

#if BUILDFLAG(IS_CHROMEOS)
void DlpPolicyEventBuilder::SetDestinationComponent(
    Component dst_component) {
  DlpPolicyEventDestination* event_destination = new DlpPolicyEventDestination;
  switch (dst_component) {
    case (Component::kArc):
      event_destination->set_component(DlpPolicyEventDestination_Component_ARC);
      break;
    case (Component::kCrostini):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_CROSTINI);
      break;
    case (Component::kPluginVm):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_PLUGIN_VM);
      break;
    case (Component::kUsb):
      event_destination->set_component(DlpPolicyEventDestination_Component_USB);
      break;
    case (Component::kDrive):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_DRIVE);
      break;
    case (Component::kOneDrive):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_ONEDRIVE);
      break;
    case (Component::kUnknownComponent):
      event_destination->set_component(
          DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT);
      break;
  }
  event.set_allocated_destination(event_destination);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void DlpPolicyEventBuilder::SetContentName(const std::string& content_name) {
  event.set_content_name(content_name);
}

DlpPolicyEvent DlpPolicyEventBuilder::Create() {
  return event;
}

void DlpPolicyEventBuilder::SetSourceUrl(const std::string& src_url) {
  DlpPolicyEventSource* event_source = new DlpPolicyEventSource;
  event_source->set_url(src_url.substr(0, content::kMaxURLDisplayChars));
  event.set_allocated_source(event_source);
}

void DlpPolicyEventBuilder::SetRestriction(
    Rule::Restriction restriction) {
  event.set_restriction(
      RuleRestriction2DlpEventRestriction(restriction));
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(src_url, rule_name, rule_id,
                                                    restriction, level);
  return event_builder->Create();
}

DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    const std::string& dst_url,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(src_url, rule_name, rule_id,
                                                    restriction, level);
  event_builder->SetDestinationUrl(dst_url);
  return event_builder->Create();
}

#if BUILDFLAG(IS_CHROMEOS)
DlpPolicyEvent CreateDlpPolicyEvent(const std::string& src_url,
                                    Component dst_component,
                                    Rule::Restriction restriction,
                                    const std::string& rule_name,
                                    const std::string& rule_id,
                                    Rule::Level level) {
  auto event_builder = DlpPolicyEventBuilder::Event(src_url, rule_name, rule_id,
                                                    restriction, level);
  event_builder->SetDestinationComponent(dst_component);

  return event_builder->Create();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

void DlpReportingManager::ReportEvent(const std::string& src_url,
                                      Rule::Restriction restriction,
                                      Rule::Level level,
                                      const std::string& rule_name,
                                      const std::string& rule_id) {
  auto event =
      CreateDlpPolicyEvent(src_url, restriction, rule_name, rule_id, level);
  ReportEvent(std::move(event));
}

void DlpReportingManager::ReportEvent(const std::string& src_url,
                                      const std::string& dst_url,
                                      Rule::Restriction restriction,
                                      Rule::Level level,
                                      const std::string& rule_name,
                                      const std::string& rule_id) {
  auto event = CreateDlpPolicyEvent(src_url, dst_url, restriction, rule_name,
                                    rule_id, level);
  ReportEvent(std::move(event));
}

#if BUILDFLAG(IS_CHROMEOS)
void DlpReportingManager::ReportEvent(const std::string& src_url,
                                      const Component dst_component,
                                      Rule::Restriction restriction,
                                      Rule::Level level,
                                      const std::string& rule_name,
                                      const std::string& rule_id) {
  auto event = CreateDlpPolicyEvent(src_url, dst_component, restriction,
                                    rule_name, rule_id, level);
  ReportEvent(std::move(event));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void DlpReportingManager::OnEventEnqueued(reporting::Status status) {
  if (!status.ok()) {
    VLOG(1) << "Could not enqueue event to DLP reporting queue because of "
            << status;
  }
  events_reported_++;
  base::UmaHistogramEnumeration(GetDlpHistogramPrefix() +
                                    dlp::kReportedEventStatus,
                                status.code(),
                                reporting::error::Code::MAX_VALUE);
}

void DlpReportingManager::ReportEvent(DlpPolicyEvent event) {
  // TODO(1187506, marcgrimme) Refactor to handle gracefully with user
  // interaction when queue is not ready.
  DlpBooleanHistogram(
      dlp::kErrorsReportQueueNotReady, !report_queue_.get());
  if (!report_queue_.get()) {
    DLOG(WARNING) << "Report queue could not be initialized. DLP reporting "
                     "functionality will be disabled.";
    return;
  }

  for (auto& observer : observers_) {
    observer.OnReportEvent(event);
  }

  reporting::ReportQueue::EnqueueCallback callback = base::BindOnce(
      &DlpReportingManager::OnEventEnqueued, weak_factory_.GetWeakPtr());

  switch (event.mode()) {
    case DlpPolicyEvent_Mode_BLOCK:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() +
              dlp::kReportedBlockLevelRestriction,
          DlpEventRestriction2RuleRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_REPORT:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() +
              dlp::kReportedReportLevelRestriction,
          DlpEventRestriction2RuleRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_WARN:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() +
              dlp::kReportedWarnLevelRestriction,
          DlpEventRestriction2RuleRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_WARN_PROCEED:
      base::UmaHistogramEnumeration(
          GetDlpHistogramPrefix() +
              dlp::kReportedWarnProceedLevelRestriction,
          DlpEventRestriction2RuleRestriction(event.restriction()));
      break;
    case DlpPolicyEvent_Mode_UNDEFINED_MODE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  report_queue_->Enqueue(std::make_unique<DlpPolicyEvent>(std::move(event)),
                         reporting::Priority::SLOW_BATCH, std::move(callback));
  VLOG(1) << "DLP event sent to reporting infrastructure.";
}

void DlpReportingManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DlpReportingManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace data_controls

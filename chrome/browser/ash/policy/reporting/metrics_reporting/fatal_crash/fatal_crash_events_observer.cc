// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;

namespace {

// Get current user session.
const ash::UserSession* GetCurrentUserSession() {
  return ash::Shell::Get()->session_controller()->GetPrimaryUserSession();
}

// Get the type of the given session.
FatalCrashTelemetry::SessionType GetSessionType(
    const ash::UserSession* user_session) {
  if (!user_session) {
    return FatalCrashTelemetry::SESSION_TYPE_UNSPECIFIED;
  }
  switch (user_session->user_info.type) {
    case user_manager::USER_TYPE_REGULAR:
      return FatalCrashTelemetry::SESSION_TYPE_REGULAR;
    case user_manager::USER_TYPE_CHILD:
      return FatalCrashTelemetry::SESSION_TYPE_CHILD;
    case user_manager::USER_TYPE_GUEST:
      return FatalCrashTelemetry::SESSION_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return FatalCrashTelemetry::SESSION_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return FatalCrashTelemetry::SESSION_TYPE_WEB_KIOSK_APP;
    default:
      NOTREACHED_NORETURN();
  }
}

// Get the user email of the given session.
absl::optional<std::string> GetUserEmail(const ash::UserSession* user_session) {
  if (!user_session || !user_session->user_info.is_managed) {
    return absl::nullopt;
  }
  if (!user_session->user_info.account_id.is_valid()) {
    LOG(ERROR) << "Invalid user account ID.";
    return absl::nullopt;
  }
  return user_session->user_info.account_id.GetUserEmail();
}
}  // namespace

FatalCrashEventsObserver::FatalCrashEventsObserver()
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this) {}

FatalCrashEventsObserver::~FatalCrashEventsObserver() = default;

void FatalCrashEventsObserver::OnEvent(
    const ash::cros_healthd::mojom::EventInfoPtr info) {
  if (!info->is_crash_event_info()) {
    return;
  }
  const auto& crash_event_info = info->get_crash_event_info();

  // TODO(b/266018440): Currently all events received by healthd are reported.
  // However, there is relatively complex logic to determine whether an event
  // should be reported.

  MetricData metric_data = FillFatalCrashTelemetry(crash_event_info);
  OnEventObserved(std::move(metric_data));
}

void FatalCrashEventsObserver::AddObserver() {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kCrash,
                         BindNewPipeAndPassRemote());
}

MetricData FatalCrashEventsObserver::FillFatalCrashTelemetry(
    const CrashEventInfoPtr& info) {
  MetricData metric_data;
  FatalCrashTelemetry& data =
      *metric_data.mutable_telemetry_data()->mutable_fatal_crash_telemetry();

  switch (info->crash_type) {
    case CrashEventInfo::CrashType::kKernel:
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_KERNEL);
      break;
    case CrashEventInfo::CrashType::kEmbeddedController:
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_EMBEDDED_CONTROLLER);
      break;
    case CrashEventInfo::CrashType::kUnknown:
      [[fallthrough]];
    default:  // Other types added by healthD that are unknown here yet.
      data.set_type(FatalCrashTelemetry::CRASH_TYPE_UNSPECIFIED);
  }

  const auto* const user_session = GetCurrentUserSession();
  if (!user_session) {
    LOG(ERROR) << "Unable to obtain user session.";
  }
  data.set_session_type(GetSessionType(user_session));
  if (auto user_email = GetUserEmail(user_session); user_email.has_value()) {
    data.mutable_affiliated_user()->set_user_email(user_email.value());
  }

  *data.mutable_local_id() = info->local_id;
  data.set_timestamp_us(info->capture_time.ToJavaTime());
  if (!info->upload_info.is_null()) {
    *data.mutable_crash_report_id() = info->upload_info->crash_report_id;
  }

  // TODO(b/266018440): was_reported_without_id is not filled. It involves logic
  // related to determining whether a crash event should be reported.

  return metric_data;
}
}  // namespace reporting

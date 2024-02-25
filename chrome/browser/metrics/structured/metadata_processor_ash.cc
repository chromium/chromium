// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/metadata_processor_ash.h"

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "metadata_processor_ash.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

bool MetadataProcessorAsh::ShouldProcessOnEventRecord(const Event& event) {
  return true;
}

void MetadataProcessorAsh::OnEventsRecord(Event* event) {}

void MetadataProcessorAsh::OnEventRecorded(StructuredEventProto* event) {
  if (event->event_type() == StructuredEventProto::SEQUENCE) {
    event->mutable_event_sequence_metadata()->set_primary_user_segment(
        GetPrimaryUserSegment());
  }
}

void MetadataProcessorAsh::OnProvideIndependentMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  auto* structured_metrics = uma_proto->mutable_structured_data();
  structured_metrics->set_device_segment(GetDeviceSegment());
}

StructuredDataProto::DeviceSegment MetadataProcessorAsh::GetDeviceSegment() {
  if (device_segment_.has_value()) {
    return device_segment_.value();
  }
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::MarketSegment segment = connector->GetEnterpriseMarketSegment();
  switch (segment) {
    case policy::MarketSegment::UNKNOWN:
      device_segment_ = StructuredDataProto::CONSUMER;
      break;
    case policy::MarketSegment::EDUCATION:
      device_segment_ = StructuredDataProto::EDUCATION;
      break;
    case policy::MarketSegment::ENTERPRISE:
      device_segment_ = StructuredDataProto::ENTERPRISE;
      break;
  }
  return device_segment_.value();
}

StructuredEventProto::PrimaryUserSegment
MetadataProcessorAsh::GetPrimaryUserSegment() {
  using UserSegment = UserTypeByDeviceTypeMetricsProvider::UserSegment;
  if (primary_user_segment_.has_value()) {
    return primary_user_segment_.value();
  } else {
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    DCHECK(primary_user);

    // If the profile isn't ready, the user's segment will be unknown.
    if (!primary_user->is_profile_created()) {
      return StructuredEventProto::UNKNOWN_PRIMARY_USER_TYPE;
    }

    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
    DCHECK(profile);

    switch (UserTypeByDeviceTypeMetricsProvider::GetUserSegment(profile)) {
      case UserSegment::kUnmanaged:
        primary_user_segment_ = StructuredEventProto::UNMANAGED;
        break;
      case UserSegment::kK12:
        primary_user_segment_ = StructuredEventProto::K12;
        break;
      case UserSegment::kUniversity:
        primary_user_segment_ = StructuredEventProto::UNIVERSITY;
        break;
      case UserSegment::kNonProfit:
        primary_user_segment_ = StructuredEventProto::NON_PROFIT;
        break;
      case UserSegment::kEnterprise:
        primary_user_segment_ = StructuredEventProto::ENTERPRISE_ORGANIZATION;
        break;
      case UserSegment::kDemoMode:
        primary_user_segment_ = StructuredEventProto::DEMO_MODE;
        break;
      case UserSegment::kKioskApp:
        primary_user_segment_ = StructuredEventProto::KIOS_APP;
        break;
      case UserSegment::kManagedGuestSession:
        primary_user_segment_ = StructuredEventProto::MANAGED_GUEST_SESSION;
        break;
    }

    return primary_user_segment_.value();
  }
}

}  // namespace metrics::structured

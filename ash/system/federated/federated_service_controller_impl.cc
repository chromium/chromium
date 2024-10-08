// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_service_controller_impl.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/login_status.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/timezone.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "chromeos/ash/services/federated/public/mojom/tables.mojom.h"
#include "components/user_manager/user_type.h"

namespace ash::federated {
namespace {

using chromeos::federated::mojom::ClientScheduleConfig;
using chromeos::federated::mojom::ClientScheduleConfigPtr;
using chromeos::federated::mojom::FederatedExampleTableId;

// Local client config struct that can be converted to mojom
// ClientScheduleConfig.
struct LocalClientConfig {
  // A client uses its client_name and launch stage to make up the task group
  // identifier when checking in with the server.
  std::string_view client_name;
  // The table_id is defined in
  // chromeos/ash/services/federated/public/mojom/tables.mojom
  FederatedExampleTableId table_id;
  // The associated_feature is usually defined in
  // ash/constants/ash_features.h/cc
  raw_ptr<const base::Feature> associated_feature;
  // The hardcoded_stage is set when a client is fully launched and no longer
  // needs to be changed, in sucn cases the associated_featur should be nullptr,
  // e.g.
  // {"foo_client_name", FederatedExampleTableId::BAR_TABLE, nullptr, "prod"}
  std::optional<std::string_view> hardcoded_stage;
};

// Each federated client should have an entry in `kLocalClientConfigs` that
// contains its client name, example table id and a feature with launch stage as
// the associated parameter, or a hardcoded launch stage.
const std::array<LocalClientConfig, 4> kLocalClientConfigs = {{
    {"input_autocorrect_phh", FederatedExampleTableId::INPUT_AUTOCORRECT,
     &features::kAutocorrectFederatedPhh},
    {"launcher_query_analytics_v1", FederatedExampleTableId::LAUNCHER_QUERY,
     nullptr},
    {"launcher_query_analytics_v2", FederatedExampleTableId::LAUNCHER_QUERY_V2,
     &features::kFederatedLauncherQueryAnalyticsVersion2Task},
    {"timezone_code_phh", FederatedExampleTableId::TIMEZONE_CODE, nullptr},
}};

// Converts a LocalClientConfig to mojom ClientScheduleConfigPtr that can be
// used for scheduling tasks via mojom interface.
// The major logic here is to obtain a valid launch stage. If a client's
// associated_feature is not null, tries to get the launch_stage from the
// feature's parameter. Otherwise tries to use the hardcoded stage. If
// eventually no valid launch_stage, the client is ignored.
std::optional<ClientScheduleConfigPtr> ConvertLocalConfigToSchedulingConfig(
    const LocalClientConfig& local_config) {
  std::string launch_stage;
  if (local_config.associated_feature != nullptr) {
    base::FeatureParam<std::string> launch_stage_param{
        local_config.associated_feature, "launch_stage", ""};
    launch_stage = launch_stage_param.Get();
  } else if (local_config.hardcoded_stage.has_value()) {
    launch_stage = local_config.hardcoded_stage.value();
  }

  if (launch_stage.empty()) {
    DVLOG(1) << "client " << local_config.client_name
             << " has no valid launch stage, ignored.";
    return std::nullopt;
  }
  auto schedule_config = ClientScheduleConfig::New();
  schedule_config->client_name = local_config.client_name;
  schedule_config->example_storage_table_id = local_config.table_id;
  schedule_config->launch_stage = launch_stage;

  return schedule_config;
}

// Prepare client configs for scheduling tasks.
std::vector<ClientScheduleConfigPtr> PrepareClientScheduleConfigs() {
  std::vector<ClientScheduleConfigPtr> client_schedule_configs;
  for (const auto& local_config : kLocalClientConfigs) {
    auto maybe_schedule_config =
        ConvertLocalConfigToSchedulingConfig(local_config);
    if (maybe_schedule_config.has_value()) {
      client_schedule_configs.push_back(
          std::move(maybe_schedule_config.value()));
    }
  }

  return client_schedule_configs;
}

chromeos::federated::mojom::ExamplePtr CreateBrellaAnalyticsExamplePtr() {
  auto example = chromeos::federated::mojom::Example::New();
  example->features = chromeos::federated::mojom::Features::New();
  auto& feature_map = example->features->feature;
  feature_map["timezone_code"] =
      federated::CreateStringList({base::CountryCodeForCurrentTimezone()});
  return example;
}

// Returns whether federated can run for this type of logged-in user.
bool IsValidPrimaryUserType(const user_manager::UserType user_type) {
  // Primary user session must have user_type = regular or child (v.s. guest,
  // public account, kiosk app).
  return user_type == user_manager::UserType::kRegular ||
         user_type == user_manager::UserType::kChild;
}

}  // namespace

FederatedServiceControllerImpl::FederatedServiceControllerImpl() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
}

FederatedServiceControllerImpl::~FederatedServiceControllerImpl() = default;

void FederatedServiceControllerImpl::OnLoginStatusChanged(
    LoginStatus login_status) {
  // Federated service daemon uses cryptohome as example store and we only
  // treat it available when a proper primary user type has signed in.

  // Actually once `federated_service_` gets bound, even if availability is
  // set false because of subsequent LoginStatus changes, it keeps bound and
  // it's safe to call `federated_service_->ReportExampleToTable()`. But on the
  // ChromeOS daemon side it loses a valid cryptohome hence no valid example
  // storage, all reported examples are abandoned.

  auto* primary_user_session =
      Shell::Get()->session_controller()->GetPrimaryUserSession();

  service_available_ =
      primary_user_session != nullptr &&
      IsValidPrimaryUserType(primary_user_session->user_info.type);

  if (service_available_ && !federated_service_.is_bound()) {
    federated::ServiceConnection::GetInstance()->BindReceiver(
        federated_service_.BindNewPipeAndPassReceiver());

    if (features::IsFederatedServiceScheduleTasksEnabled()) {
      federated_service_->StartSchedulingWithConfig(
          PrepareClientScheduleConfigs());
    }

    // On session first login, reports one example for "timezone_code_phh", a
    // trivial F.A. task for prove-out purpose.
    if (!reported_) {
      federated_service_->ReportExampleToTable(
          FederatedExampleTableId::TIMEZONE_CODE,
          CreateBrellaAnalyticsExamplePtr());
      reported_ = true;
    }
  }
}

bool FederatedServiceControllerImpl::IsServiceAvailable() const {
  return service_available_;
}

}  // namespace ash::federated

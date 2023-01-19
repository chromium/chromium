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
#include "base/no_destructor.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "components/user_manager/user_type.h"

namespace ash::federated {
namespace {

// New clients should define their switches in ash/constants/ash_features.h/cc,
// and adds entries {client_name, switch} to `kClientFeatureMap`.
const auto kClientFeatureMap =
    base::MakeFixedFlatMap<std::string, const base::Feature*>({
        {"timezone_code_phh", &features::kFederatedTimezoneCodePhh},
    });

// A client with empty launch stage ("") in the return value means
// federated-service will not schedule tasks for it. Therefore, if a client's
// feature is disabled, we set an empty stage for it to respect "disable".
// While if the feature is enabled, we expect the client has a non-empty launch
// stage, otherwise we drop it from the return value, leaving it a
// federated-service side decision (federated-service has hardcoded launch stage
// for each client).
base::flat_map<std::string, std::string> GetClientLaunchStage() {
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      client_launch_stage_map([] {
        base::flat_map<std::string, std::string> map;
        for (const auto& kv : kClientFeatureMap) {
          if (!base::FeatureList::IsEnabled(*kv.second)) {
            map[kv.first] = "";
          } else {
            base::FeatureParam<std::string> launch_stage{kv.second,
                                                         "launch_stage", ""};
            if (!launch_stage.Get().empty()) {
              map[kv.first] = launch_stage.Get();
            }
          }
        }
        return map;
      }());

  return *client_launch_stage_map;
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
  return user_type == user_manager::USER_TYPE_REGULAR ||
         user_type == user_manager::USER_TYPE_CHILD;
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
  // Federated service daemon uses cryptohome as example store and we only treat
  // it available when a proper primary user type has signed in.

  // Actually once `federated_service_` gets bound, even if availability is set
  // false because of subsequent LoginStatus changes, it keeps bound and it's
  // safe to call `federated_service_->ReportExample()`. But on the ChromeOS
  // daemon side it loses a valid ctyptohome hence no valid example storage, all
  // reported examples are abandoned.

  auto* primary_user_session =
      Shell::Get()->session_controller()->GetPrimaryUserSession();

  service_available_ =
      primary_user_session != nullptr &&
      IsValidPrimaryUserType(primary_user_session->user_info.type);

  if (service_available_ && !federated_service_.is_bound()) {
    federated::ServiceConnection::GetInstance()->BindReceiver(
        federated_service_.BindNewPipeAndPassReceiver());

    if (features::IsFederatedServiceScheduleTasksEnabled()) {
      federated_service_->StartScheduling(GetClientLaunchStage());
    }

    // On session first login, reports one example for "timezone_code_phh", a
    // trivial F.A. task for prove-out purpose.
    if (!reported_) {
      federated_service_->ReportExample("timezone_code_phh",
                                        CreateBrellaAnalyticsExamplePtr());
      reported_ = true;
    }
  }
}

bool FederatedServiceControllerImpl::IsServiceAvailable() const {
  return service_available_;
}

}  // namespace ash::federated

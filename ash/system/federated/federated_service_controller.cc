// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_service_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/login_status.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/i18n/timezone.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "components/user_manager/user_type.h"

namespace ash::federated {
namespace {

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

FederatedServiceController::FederatedServiceController() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
}

FederatedServiceController::~FederatedServiceController() = default;

void FederatedServiceController::OnLoginStatusChanged(
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
      federated_service_->StartScheduling();
    }

    // On session first login, reports one example for
    // "timezone_code_population", a trivial F.A. task for prove-out purpose.
    if (!reported_) {
      federated_service_->ReportExample("timezone_code_population",
                                        CreateBrellaAnalyticsExamplePtr());
      reported_ = true;
    }
  }
}

}  // namespace ash::federated

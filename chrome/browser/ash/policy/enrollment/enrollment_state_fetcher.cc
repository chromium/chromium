// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"

#include <memory>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

using private_membership::rlwe::RlwePlaintextId;

namespace policy {
namespace {

namespace em = enterprise_management;

// TODO(b/265923216): Wrap callbacks into an object ensuring they are called.

RlwePlaintextId ConstructPlainttextId(const std::string& rlz_brand_code,
                                      const std::string& serial_number) {
  RlwePlaintextId rlwe_id;
  // See http://shortn/_tkT6f7xV0F for format specification.
  const std::string rlz_brand_code_hex =
      base::HexEncode(rlz_brand_code.data(), rlz_brand_code.size());
  const std::string id = rlz_brand_code_hex + "/" + serial_number;
  // The PSM client library, which consumes this proto, will hash non-sensitive
  // identifier and truncate to a few bits before sending it to the server,
  // ensuring privacy. Hence, we can use the same identifier for both fields.
  rlwe_id.set_non_sensitive_id(id);
  rlwe_id.set_sensitive_id(id);

  return rlwe_id;
}

// The DeterminationContext is used to store state and cache computed values
// used at various steps of the enrollment state fetch sequence.
struct DeterminationContext {
  // Constructs client to communicate with PSM servers.
  // Must be set before sequence starts.
  EnrollmentStateFetcher::RlweClientFactory rlwe_client_factory;

  // Allows retrieving system values from multiple sources.
  // Must be set before sequence starts.
  raw_ptr<ash::system::StatisticsProvider, ExperimentalAsh> statistics_provider;

  // Interface for talking to DMServer.
  // Must be set before sequence starts.
  raw_ptr<DeviceManagementService, ExperimentalAsh> device_management_service =
      nullptr;

  // This will be used to configure `job`s for the `device_management_service`.
  // Must be set before sequence starts.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;

  // Interface for retrieving synchronized clock time.
  // Must be set before sequence starts.
  raw_ptr<ash::SystemClockClient, ExperimentalAsh> system_clock_client;

  // Used to retrieve device state keys.
  // Must be set before sequence starts.
  raw_ptr<ServerBackedStateKeysBroker, ExperimentalAsh> state_key_broker;

  // Interface for checking ownership.
  // Must be set before sequence starts.
  raw_ptr<ash::DeviceSettingsService, ExperimentalAsh> device_settings_service;

  // RLZ brand code and serial numbers retrieved using `statistics_provider`.
  // Used for state availability determination (PSM) and state retrieval
  // requests. Computed by DeviceIdentifiers step.
  std::string rlz_brand_code;
  std::string serial_number;

  // State key retrieved from session_manager. Used for state retrieval
  // request. Computed by StateKeys step.
  absl::optional<std::string> state_key;

  // Maintains the required data and methods to communicate with the PSM
  // server. In particular, it holds the plaintext id we are want to check
  // membership for. Must be set before RlweOprf and RlweQuery steps.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client;
};

void StorePsmError(PrefService* local_state) {
  local_state->SetInteger(prefs::kEnrollmentPsmResult,
                          em::DeviceRegisterRequest::PSM_RESULT_ERROR);
}

// Class to synchronize the system clock.
//
// This is a step in enrollment state fetch (see Sequence class below).
class SystemClock {
  static constexpr base::TimeDelta kSystemClockSyncWaitTimeout =
      base::Seconds(45);

 public:
  SystemClock() = default;
  SystemClock(const SystemClock&) = delete;
  SystemClock& operator=(const SystemClock&) = delete;

  // This will attempt to synchronize the system clock within up to
  // `kSystemClockSyncWaitTimeout`.
  // It will report success (`true`) or failure (`false`) via the
  // `completion_callback`.
  void Sync(ash::SystemClockClient* system_clock_client,
            base::OnceCallback<void(bool)> completion_callback) {
    system_clock_sync_observation_ =
        ash::SystemClockSyncObservation::WaitForSystemClockSync(
            system_clock_client, kSystemClockSyncWaitTimeout,
            std::move(completion_callback));
  }

  // Utility for waiting until the system clock has been synchronized.
  std::unique_ptr<ash::SystemClockSyncObservation>
      system_clock_sync_observation_;
};

// Class to check device ownership.
//
// This is a step in enrollment state fetch (see Sequence class below).
class Ownership {
 public:
  Ownership() = default;
  Ownership(const Ownership&) = delete;
  Ownership& operator=(const Ownership&) = delete;

  // This will attempt to check device ownership. It will report the result via
  // the `completion_callback`.
  void Check(
      ash::DeviceSettingsService* device_settings_service,
      base::OnceCallback<void(ash::DeviceSettingsService::OwnershipStatus)>
          completion_callback) {
    // TODO(b/278056625): Skip state fetch when install attributes are locked.
    device_settings_service->GetOwnershipStatusAsync(
        std::move(completion_callback));
  }
};

// Class to check whether embargo date has passed.
//
// Must be used only after system clock has been synchronized.
// This is a step in enrollment state fetch (see Sequence class below).
class EmbargoDate {
 public:
  EmbargoDate() = default;
  EmbargoDate(const EmbargoDate&) = delete;
  EmbargoDate& operator=(const EmbargoDate&) = delete;

  bool Passed(DeterminationContext& context) {
    const ash::system::FactoryPingEmbargoState embargo_state =
        ash::system::GetEnterpriseManagementPingEmbargoState(
            context.statistics_provider);
    if (embargo_state == ash::system::FactoryPingEmbargoState::kNotPassed) {
      LOG(WARNING) << "Embargo date not passed";
      return false;
    }
    // When embargo date is missing, malformed or invalid, we assume it has
    // passed and proceed with the enrollment.
    return true;
  }
};

// Class to obtain brand code and serial number.
//
// This is a step in enrollment state fetch (see Sequence class below).
class DeviceIdentifiers {
 public:
  DeviceIdentifiers() = default;
  DeviceIdentifiers(const DeviceIdentifiers&) = delete;
  DeviceIdentifiers& operator=(const DeviceIdentifiers&) = delete;

  // Retrieves brand code and serial numbers.
  //
  // On success, stores retrieved identifiers in `rlz_brand_code` and
  // `serial_number` and returns true.
  bool Retrieve(ash::system::StatisticsProvider* statistics_provider,
                std::string& out_rlz_brand_code,
                std::string& out_serial_number) {
    out_rlz_brand_code = std::string(
        statistics_provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey)
            .value_or(""));
    out_serial_number =
        std::string(statistics_provider->GetMachineID().value_or(""));
    ReportDeviceIdentifierStatus(out_serial_number.empty(),
                                 out_rlz_brand_code.empty());
    return !out_serial_number.empty() && !out_rlz_brand_code.empty();
  }

 private:
  static void ReportDeviceIdentifierStatus(bool serial_number_missing,
                                           bool rlz_brand_code_missing) {
    enum class DeviceIdentifierStatus {
      // These values are persisted to logs. Entries should not be renumbered
      // and numeric values should never be reused.
      kAllPresent = 0,
      kSerialNumberMissing = 1,
      kRlzBrandCodeMissing = 2,
      kAllMissing = 3,
      kMaxValue = kAllMissing
    };

    if (serial_number_missing && rlz_brand_code_missing) {
      base::UmaHistogramEnumeration(
          kUMAStateDeterminationDeviceIdentifierStatus,
          DeviceIdentifierStatus::kAllMissing);
    } else if (serial_number_missing) {
      base::UmaHistogramEnumeration(
          kUMAStateDeterminationDeviceIdentifierStatus,
          DeviceIdentifierStatus::kSerialNumberMissing);
    } else if (rlz_brand_code_missing) {
      base::UmaHistogramEnumeration(
          kUMAStateDeterminationDeviceIdentifierStatus,
          DeviceIdentifierStatus::kRlzBrandCodeMissing);
    } else {
      base::UmaHistogramEnumeration(
          kUMAStateDeterminationDeviceIdentifierStatus,
          DeviceIdentifierStatus::kAllPresent);
    }
  }
};

// Class to send RLWE OPRF request as part of PSM protocol.
//
// This is a step in enrollment state fetch (see Sequence class below).
class RlweOprf {
 public:
  using Response = private_membership::rlwe::PrivateMembershipRlweOprfResponse;
  using Result = base::expected<Response, AutoEnrollmentState>;
  using CompletionCallback = base::OnceCallback<void(Result)>;

  RlweOprf() = default;
  RlweOprf(const RlweOprf&) = delete;
  RlweOprf& operator=(const RlweOprf&) = delete;

  void Request(DeterminationContext& context,
               CompletionCallback completion_callback) {
    DCHECK(completion_callback);

    context.psm_rlwe_client = context.rlwe_client_factory.Run(
        private_membership::rlwe::CROS_DEVICE_STATE_UNIFIED,
        ConstructPlainttextId(context.rlz_brand_code, context.serial_number));
    const auto oprf_request = context.psm_rlwe_client->CreateOprfRequest();
    if (!oprf_request.ok()) {
      LOG(ERROR) << "Failed to create PSM RLWE OPRF request: "
                 << oprf_request.status();
      return std::move(completion_callback)
          .Run(base::unexpected(AutoEnrollmentState::kNoEnrollment));
    }

    // Prepare the RLWE OPRF request job.
    auto config = std::make_unique<DMServerJobConfiguration>(
        context.device_management_service,
        DeviceManagementService::JobConfiguration::
            TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/absl::nullopt, context.url_loader_factory,
        base::BindOnce(&RlweOprf::OnRequestDone, weak_factory_.GetWeakPtr(),
                       std::move(completion_callback)));

    *config->request()
         ->mutable_private_set_membership_request()
         ->mutable_rlwe_request()
         ->mutable_oprf_request() = *oprf_request;

    VLOG(1) << "Send PSM RLWE OPRF request";
    job_ = context.device_management_service->CreateJob(std::move(config));
  }

 private:
  void OnRequestDone(CompletionCallback completion_callback,
                     DMServerJobResult result) {
    // Handle errors
    base::UmaHistogramSparse(
        kUMAStateDeterminationPsmRlweOprfRequestDmStatusCode, result.dm_status);
    base::UmaHistogramSparse(
        kUMAStateDeterminationPsmRlweOprfRequestNetworkErrorCode,
        -result.net_error);
    switch (result.dm_status) {
      case DM_STATUS_SUCCESS: {
        if (!result.response.has_private_set_membership_response() ||
            !result.response.private_set_membership_response()
                 .has_rlwe_response() ||
            !result.response.private_set_membership_response()
                 .rlwe_response()
                 .has_oprf_response()) {
          LOG(ERROR) << "Empty PSM RLWE OPRF response";
          return std::move(completion_callback)
              .Run(base::unexpected(AutoEnrollmentState::kServerError));
        }
        break;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR) << "PSM RLWE OPRF connection error";
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kConnectionError));
      }
      default: {
        LOG(ERROR) << "PSM RLWE OPRF server error: " << result.dm_status;
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kServerError));
      }
    }

    // Handle success
    VLOG(1) << "PSM RLWE OPRF request completed successfully";
    return std::move(completion_callback)
        .Run(base::ok(result.response.private_set_membership_response()
                          .rlwe_response()
                          .oprf_response()));
  }

 private:
  std::unique_ptr<DeviceManagementService::Job> job_;
  base::WeakPtrFactory<RlweOprf> weak_factory_{this};
};

// Class to send RLWE Query request as part of PSM protocol.
//
// This is a step in enrollment state fetch (see Sequence class below).
class RlweQuery {
 public:
  RlweQuery() = default;
  RlweQuery(const RlweQuery&) = delete;
  RlweQuery& operator=(const RlweQuery&) = delete;

  using Result = base::expected<bool, AutoEnrollmentState>;
  using CompletionCallback =
      base::OnceCallback<void(base::expected<bool, AutoEnrollmentState>)>;

  void Request(
      DeterminationContext& context,
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response,
      CompletionCallback completion_callback) {
    DCHECK(completion_callback);
    DCHECK(context.psm_rlwe_client);
    const auto query_request =
        context.psm_rlwe_client->CreateQueryRequest(oprf_response);

    if (!query_request.ok()) {
      LOG(ERROR) << "Failed to create PSM RLWE query request: "
                 << query_request.status();
      return std::move(completion_callback)
          .Run(base::unexpected(AutoEnrollmentState::kNoEnrollment));
    }

    // Prepare the RLWE query request job.
    auto config = std::make_unique<DMServerJobConfiguration>(
        context.device_management_service,
        DeviceManagementService::JobConfiguration::
            TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/absl::nullopt, context.url_loader_factory,
        base::BindOnce(&RlweQuery::OnRequestDone, weak_factory_.GetWeakPtr(),
                       base::Unretained(context.psm_rlwe_client.get()),
                       std::move(completion_callback)));

    *config->request()
         ->mutable_private_set_membership_request()
         ->mutable_rlwe_request()
         ->mutable_query_request() = *query_request;

    VLOG(1) << "Send PSM RLWE query request";
    job_ = context.device_management_service->CreateJob(std::move(config));
  }

  void OnRequestDone(
      private_membership::rlwe::PrivateMembershipRlweClient* psm_rlwe_client,
      CompletionCallback completion_callback,
      DMServerJobResult result) {
    // Handle errors
    base::UmaHistogramSparse(
        kUMAStateDeterminationPsmRlweQueryRequestDmStatusCode,
        result.dm_status);
    base::UmaHistogramSparse(
        kUMAStateDeterminationPsmRlweQueryRequestNetworkErrorCode,
        -result.net_error);
    switch (result.dm_status) {
      case DM_STATUS_SUCCESS: {
        // Check if the RLWE query response is empty.
        if (!result.response.has_private_set_membership_response() ||
            !result.response.private_set_membership_response()
                 .has_rlwe_response() ||
            !result.response.private_set_membership_response()
                 .rlwe_response()
                 .has_query_response()) {
          LOG(ERROR) << "Empty PSM RLWE query response";
          return std::move(completion_callback)
              .Run(base::unexpected(AutoEnrollmentState::kServerError));
        }
        break;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR) << "PSM RLWE query connection error";
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kConnectionError));
      }
      default: {
        LOG(ERROR) << "PSM RLWE query server error: " << result.dm_status;
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kServerError));
      }
    }

    const auto responses = psm_rlwe_client->ProcessQueryResponse(
        result.response.private_set_membership_response()
            .rlwe_response()
            .query_response());

    if (!responses.ok() || responses->membership_responses_size() != 1) {
      LOG(ERROR) << "Invalid PSM RLWE query response";
      return std::move(completion_callback)
          .Run(base::unexpected(AutoEnrollmentState::kServerError));
    }

    if (responses->membership_responses_size() != 1) {
      LOG(ERROR) << "Invalid PSM RLWE query response: "
                 << responses->membership_responses_size()
                 << " membership responses, expected 1";
      return std::move(completion_callback)
          .Run(base::unexpected(AutoEnrollmentState::kServerError));
    }

    const bool is_member =
        responses->membership_responses(0).membership_response().is_member();
    return std::move(completion_callback).Run(base::ok(is_member));
  }

  void StoreResponse(PrefService* local_state, bool is_member) {
    local_state->SetTime(prefs::kEnrollmentPsmDeterminationTime,
                         base::Time::Now());
    local_state->SetInteger(
        prefs::kEnrollmentPsmResult,
        is_member
            ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
            : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  }

 private:
  std::unique_ptr<DeviceManagementService::Job> job_;
  base::WeakPtrFactory<RlweQuery> weak_factory_{this};
};

// Class to obtain state keys.
//
// This is a step in enrollment state fetch (see Sequence class below).
class StateKeys {
  static constexpr int kMaxAttempts = 10;

 public:
  StateKeys() = default;
  StateKeys(const StateKeys&) = delete;
  StateKeys& operator=(const StateKeys&) = delete;

  using CompletionCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;

  // This will try up to `kMaxAttempts` times to obtain the state keys.  If
  // successful, it will return the current state key by calling the completion
  // callback.
  // Otherwise, it will return `absl::nullopt`.
  void Retrieve(ServerBackedStateKeysBroker* state_key_broker,
                CompletionCallback completion_callback) {
    ++attempts_;
    state_key_broker->RequestStateKeys(base::BindOnce(
        &StateKeys::OnStateKeysRetrieved, weak_factory_.GetWeakPtr(),
        state_key_broker, std::move(completion_callback)));
  }

 private:
  void OnStateKeysRetrieved(ServerBackedStateKeysBroker* state_key_broker,
                            CompletionCallback completion_callback,
                            const std::vector<std::string>& state_keys) {
    if (state_keys.empty() || state_keys[0].empty()) {
      if (attempts_ >= kMaxAttempts) {
        return std::move(completion_callback).Run(absl::nullopt);
      }
      return Retrieve(state_key_broker, std::move(completion_callback));
    }
    return std::move(completion_callback).Run(state_keys[0]);
  }

  int attempts_ = 0;
  base::WeakPtrFactory<StateKeys> weak_factory_{this};
};

// Class to send state request to DMServer.
//
// This is a step in enrollment state fetch (see Sequence class below).
class EnrollmentState {
 public:
  struct Response {
    base::Value::Dict dict;
    AutoEnrollmentState state = AutoEnrollmentState::kPending;
  };
  using Result = base::expected<Response, AutoEnrollmentState>;
  using CompletionCallback = base::OnceCallback<void(Result)>;

  EnrollmentState() = default;
  EnrollmentState(const EnrollmentState&) = delete;
  EnrollmentState& operator=(const EnrollmentState&) = delete;

  void Request(DeterminationContext& context,
               CompletionCallback completion_callback) {
    // TODO(b/265923216): Replace this with unified request type.
    auto config = std::make_unique<DMServerJobConfiguration>(
        context.device_management_service,
        DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL,
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/absl::nullopt, context.url_loader_factory,
        base::BindOnce(&EnrollmentState::OnRequestDone,
                       weak_factory_.GetWeakPtr(),
                       std::move(completion_callback)));

    auto* request = config->request()->mutable_device_state_retrieval_request();
    if (context.state_key.has_value()) {
      request->set_server_backed_state_key(context.state_key.value());
    }
    request->set_brand_code(std::string(context.rlz_brand_code));
    request->set_serial_number(std::string(context.serial_number));

    VLOG(1) << "Send unified enrollment state retrieval request";
    job_ = context.device_management_service->CreateJob(std::move(config));
  }

  void OnRequestDone(CompletionCallback completion_callback,
                     DMServerJobResult result) {
    // Handle errors
    base::UmaHistogramSparse(kUMAStateDeterminationStateRequestDmStatusCode,
                             result.dm_status);
    base::UmaHistogramSparse(kUMAStateDeterminationStateRequestNetworkErrorCode,
                             -result.net_error);
    switch (result.dm_status) {
      case DM_STATUS_SUCCESS: {
        if (!result.response.has_device_state_retrieval_response()) {
          LOG(ERROR) << "Server failed to provide unified enrollment response.";
          return std::move(completion_callback)
              .Run(base::unexpected(AutoEnrollmentState::kServerError));
        }
        break;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR) << "Enrollment state query connection error";
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kConnectionError));
      }
      default: {
        LOG(ERROR) << "Enrollment state query server error";
        return std::move(completion_callback)
            .Run(base::unexpected(AutoEnrollmentState::kServerError));
      }
    }

    const auto state_response =
        result.response.device_state_retrieval_response();

    if (state_response.restore_mode() ==
        em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE) {
      ParseInitialStateResponse(state_response.initial_state_response(),
                                std::move(completion_callback));
    } else {
      ParseSecondaryStateResponse(state_response,
                                  std::move(completion_callback));
    }
  }

  void ParseInitialStateResponse(
      const em::DeviceInitialEnrollmentStateResponse& state_response,
      CompletionCallback completion_callback) {
    Response response;
    std::string mode;
    std::tie(response.state, mode) =
        ConvertInitialEnrollmentMode(state_response.initial_enrollment_mode());
    if (!mode.empty()) {
      response.dict.Set(kDeviceStateMode, mode);
    }

    if (state_response.has_management_domain()) {
      response.dict.Set(kDeviceStateManagementDomain,
                        state_response.management_domain());
    }

    if (state_response.has_is_license_packaged_with_device()) {
      response.dict.Set(kDeviceStatePackagedLicense,
                        state_response.is_license_packaged_with_device());
    }

    if (state_response.has_license_packaging_sku()) {
      response.dict.Set(
          kDeviceStateLicenseType,
          ConvertLicenseType(state_response.license_packaging_sku()));
    }

    if (state_response.has_assigned_upgrade_type()) {
      response.dict.Set(
          kDeviceStateAssignedUpgradeType,
          ConvertAssignedUpgradeType(state_response.assigned_upgrade_type()));
    }

    if (state_response.has_disabled_state()) {
      response.dict.Set(kDeviceStateDisabledMessage,
                        state_response.disabled_state().message());
    }

    VLOG(1) << "Initial enrollment mode = '" << mode << "', "
            << (state_response.is_license_packaged_with_device() ? "with"
                                                                 : "no")
            << " packaged license.";

    return std::move(completion_callback).Run(base::ok(std::move(response)));
  }

  void ParseSecondaryStateResponse(
      const em::DeviceStateRetrievalResponse& state_response,
      CompletionCallback completion_callback) {
    Response response;
    std::string mode;
    std::tie(response.state, mode) =
        ConvertRestoreMode(state_response.restore_mode());
    if (!mode.empty()) {
      response.dict.Set(kDeviceStateMode, mode);
    }

    if (state_response.has_management_domain()) {
      response.dict.Set(kDeviceStateManagementDomain,
                        state_response.management_domain());
    }

    if (state_response.has_disabled_state()) {
      response.dict.Set(kDeviceStateDisabledMessage,
                        state_response.disabled_state().message());
    }

    if (ash::features::IsAutoEnrollmentKioskInOobeEnabled() &&
        state_response.has_license_type()) {
      response.dict.Set(kDeviceStateLicenseType,
                        ConvertAutoEnrollmentLicenseType(
                            state_response.license_type().license_type()));
    }

    VLOG(1) << "Received restore mode " << mode;
    return std::move(completion_callback).Run(base::ok(std::move(response)));
  }

  void StoreResponse(PrefService* local_state, const base::Value::Dict& dict) {
    VLOG(1) << "ServerBackedDeviceState pref: " << dict;
    local_state->SetDict(prefs::kServerBackedDeviceState, dict.Clone());
  }

 private:
  // Converts an initial enrollment mode enum value from the DM protocol for
  // initial enrollment into the corresponding prefs string constant.
  std::pair<AutoEnrollmentState, std::string> ConvertInitialEnrollmentMode(
      em::DeviceInitialEnrollmentStateResponse ::InitialEnrollmentMode
          initial_enrollment_mode) {
    using Response = em::DeviceInitialEnrollmentStateResponse;

    switch (initial_enrollment_mode) {
      case Response::INITIAL_ENROLLMENT_MODE_NONE:
        return {AutoEnrollmentState::kNoEnrollment, std::string()};
      case Response::INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED:
        return {AutoEnrollmentState::kEnrollment,
                kDeviceStateInitialModeEnrollmentEnforced};
      case Response::INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED:
        return {AutoEnrollmentState::kEnrollment,
                kDeviceStateInitialModeEnrollmentZeroTouch};
      case Response::INITIAL_ENROLLMENT_MODE_DISABLED:
        return {AutoEnrollmentState::kDisabled, kDeviceStateModeDisabled};
    }
  }

  // Converts a license packaging sku enum value from the DM protocol for
  // initial enrollment into the corresponding prefs string constant.
  std::string ConvertLicenseType(
      em::DeviceInitialEnrollmentStateResponse ::LicensePackagingSKU
          license_sku) {
    using Response = em::DeviceInitialEnrollmentStateResponse;
    switch (license_sku) {
      case Response::NOT_EXIST:
        return std::string();
      case Response::CHROME_ENTERPRISE:
        return kDeviceStateLicenseTypeEnterprise;
      case Response::CHROME_EDUCATION:
        return kDeviceStateLicenseTypeEducation;
      case Response::CHROME_TERMINAL:
        return kDeviceStateLicenseTypeTerminal;
    }
  }

  // Converts an assigned upgrade type enum value from the DM protocol for
  // initial enrollment into the corresponding prefs string constant.
  std::string ConvertAssignedUpgradeType(
      em::DeviceInitialEnrollmentStateResponse::AssignedUpgradeType
          assigned_upgrade_type) {
    switch (assigned_upgrade_type) {
      case em::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_UNSPECIFIED:
        return std::string();
      case em::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_CHROME_ENTERPRISE:
        return kDeviceStateAssignedUpgradeTypeChromeEnterprise;
      case em::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_KIOSK_AND_SIGNAGE:
        return kDeviceStateAssignedUpgradeTypeKiosk;
    }
  }

  // Converts a restore mode enum value from the DM protocol for FRE into the
  // corresponding prefs string constant.
  std::pair<AutoEnrollmentState, std::string> ConvertRestoreMode(
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
    switch (restore_mode) {
      case em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE:
        return {AutoEnrollmentState::kNoEnrollment, std::string()};
      case em::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED:
        return {AutoEnrollmentState::kEnrollment,
                kDeviceStateRestoreModeReEnrollmentRequested};
      case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED:
        return {AutoEnrollmentState::kEnrollment,
                kDeviceStateRestoreModeReEnrollmentEnforced};
      case em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED:
        return {AutoEnrollmentState::kDisabled, kDeviceStateModeDisabled};
      case em::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
        return {AutoEnrollmentState::kEnrollment,
                kDeviceStateRestoreModeReEnrollmentZeroTouch};
    }
  }

  // Converts a enterprise_management::LicenseType_LicenseTypeEnum
  // for AutoEnrollment to it corresponding string.
  std::string ConvertAutoEnrollmentLicenseType(
      em::LicenseType_LicenseTypeEnum license_type) {
    switch (license_type) {
      case em::LicenseType::UNDEFINED:
        return std::string();
      case em::LicenseType::CDM_PERPETUAL:
        return kDeviceStateLicenseTypeEnterprise;
      case em::LicenseType::CDM_ANNUAL:
        return kDeviceStateLicenseTypeEnterprise;
      case em::LicenseType::KIOSK:
        return kDeviceStateLicenseTypeTerminal;
      case em::LicenseType::CDM_PACKAGED:
        return kDeviceStateLicenseTypeEnterprise;
    }
  }

  std::unique_ptr<DeviceManagementService::Job> job_;
  base::WeakPtrFactory<EnrollmentState> weak_factory_{this};
};

class EnrollmentStateFetcherImpl : public EnrollmentStateFetcher {
 public:
  EnrollmentStateFetcherImpl(
      base::OnceCallback<void(AutoEnrollmentState)> report_result,
      PrefService* local_state,
      RlweClientFactory rlwe_client_factory,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ash::SystemClockClient* system_clock_client,
      ServerBackedStateKeysBroker* state_key_broker,
      ash::DeviceSettingsService* device_settings_service) {
    DCHECK(report_result);
    DCHECK(local_state);
    DCHECK(rlwe_client_factory);
    DCHECK(device_management_service);
    DCHECK(url_loader_factory);
    DCHECK(system_clock_client);
    DCHECK(state_key_broker);
    DCHECK(device_settings_service);

    call_sequence_ = std::make_unique<Sequence>(
        std::move(report_result), local_state,
        DeterminationContext{std::move(rlwe_client_factory),
                             ash::system::StatisticsProvider::GetInstance(),
                             device_management_service, url_loader_factory,
                             system_clock_client, state_key_broker,
                             device_settings_service});
  }

  void Start() override;

  ~EnrollmentStateFetcherImpl() override = default;

 private:
  class Sequence;
  std::unique_ptr<Sequence> call_sequence_;
};

// This implements a strict sequence of asynchronous calls:
//   - synchronize clock
//   - check embargo date
//   - retrieve device identifiers (brand code and serial number)
//   - PSM OPRF
//   - PSM Query
//   - obtain state keys
//   - DeviceStateRetrievalRequest
class EnrollmentStateFetcherImpl::Sequence {
 public:
  Sequence(base::OnceCallback<void(AutoEnrollmentState)> report_result,
           PrefService* local_state,
           DeterminationContext context)
      : report_result_(std::move(report_result)),
        local_state_(local_state),
        context_(std::move(context)) {}

  void Start() {
    fetch_started_ = base::TimeTicks::Now();
    const bool enabled =
        AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();
    base::UmaHistogramBoolean(kUMAStateDeterminationEnabled, enabled);
    if (!enabled) {
      VLOG(1) << "Unified state determination is disabled";
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }

    // Flex devices do not support FRE, hence there is no need to perform state
    // determination. Users are still able to manually enroll devices though.
    const bool is_on_flex = ash::switches::IsRevenBranding();
    base::UmaHistogramBoolean(kUMAStateDeterminationOnFlex, is_on_flex);
    if (is_on_flex) {
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }
    // TODO(b/265923216): Investigate the possibility of using bypassing PSM and
    // using state key to directly request state when identifiers are missing.
    if (!device_identifiers_.Retrieve(context_.statistics_provider,
                                      context_.rlz_brand_code,
                                      context_.serial_number)) {
      // Skip enrollment if serial number or brand code are missing.
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }

    step_started_ = base::TimeTicks::Now();
    system_clock_.Sync(context_.system_clock_client,
                       base::BindOnce(&Sequence::OnSystemClockSynced,
                                      weak_factory_.GetWeakPtr()));
  }

 private:
  void OnSystemClockSynced(bool synchronized) {
    ReportStepDurationAndResetTimer(kUMASuffixSystemClockSync);
    base::UmaHistogramBoolean(kUMAStateDeterminationSystemClockSynchronized,
                              synchronized);
    if (!synchronized) {
      LOG(ERROR) << "System clock failed to synchronize";
      return ReportResult(AutoEnrollmentState::kConnectionError);
    }

    const bool passed = embargo_date_.Passed(context_);
    base::UmaHistogramBoolean(kUMAStateDeterminationEmbargoDatePassed, passed);
    if (!passed) {
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }

    ownership_.Check(context_.device_settings_service,
                     base::BindOnce(&Sequence::OnOwnershipChecked,
                                    weak_factory_.GetWeakPtr()));
  }

  void OnOwnershipChecked(ash::DeviceSettingsService::OwnershipStatus status) {
    ReportStepDurationAndResetTimer(kUMASuffixOwnershipCheck);
    base::UmaHistogramEnumeration(kUMAStateDeterminationOwnershipStatus,
                                  status);
    if (status ==
        ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown) {
      LOG(ERROR) << "Device ownership is unknown. Skipping enrollment";
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }

    if (status ==
        ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
      VLOG(1) << "Device ownership is already taken. Skipping enrollment";
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }

    oprf_.Request(context_, base::BindOnce(&Sequence::OnOprfRequestDone,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnOprfRequestDone(RlweOprf::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixOPRFRequest);
    if (!result.has_value()) {
      StorePsmError(local_state_);
      return ReportResult(result.error());
    }
    query_.Request(context_, result.value(),
                   base::BindOnce(&Sequence::OnQueryRequestDone,
                                  weak_factory_.GetWeakPtr()));
  }

  void OnQueryRequestDone(RlweQuery::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixQueryRequest);
    if (!result.has_value()) {
      StorePsmError(local_state_);
      return ReportResult(result.error());
    }

    RlwePlaintextId psm_id =
        ConstructPlainttextId(context_.rlz_brand_code, context_.serial_number);
    // Use WARNING level to preserve PSM ID in the logs.
    LOG(WARNING) << "PSM determination successful. Identifier "
                 << psm_id.sensitive_id() << " is "
                 << (result.value() ? "" : " not") << " present on the server";

    base::UmaHistogramBoolean(kUMAStateDeterminationPsmReportedAvailableState,
                              result.value());
    if (!result.value()) {
      return ReportResult(AutoEnrollmentState::kNoEnrollment);
    }
    query_.StoreResponse(local_state_, result.value());
    state_keys_.Retrieve(context_.state_key_broker,
                         base::BindOnce(&Sequence::OnStateKeysRetrieved,
                                        weak_factory_.GetWeakPtr()));
  }

  void OnStateKeysRetrieved(absl::optional<std::string> state_key) {
    ReportStepDurationAndResetTimer(kUMASuffixStateKeyRetrieval);
    base::UmaHistogramBoolean(kUMAStateDeterminationStateKeysRetrieved,
                              state_key.has_value());
    LOG_IF(WARNING, !state_key) << "Failed to obtain state keys";
    context_.state_key = state_key;
    state_.Request(context_, base::BindOnce(&Sequence::OnStateRequestDone,
                                            weak_factory_.GetWeakPtr()));
  }

  void OnStateRequestDone(EnrollmentState::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixStateRequest);
    base::UmaHistogramBoolean(kUMAStateDeterminationStateReturned,
                              result.has_value());
    if (!result.has_value()) {
      return ReportResult(result.error());
    }
    state_.StoreResponse(local_state_, result->dict);
    return ReportResult(result->state);
  }

  // Helpers
  void ReportTotalDuration(base::TimeDelta fetch_duration,
                           AutoEnrollmentState state) {
    std::string uma_suffix;
    switch (state) {
      case AutoEnrollmentState::kIdle:
      case AutoEnrollmentState::kPending:
        NOTREACHED();
        break;
      case AutoEnrollmentState::kConnectionError:
        uma_suffix = kUMASuffixConnectionError;
        break;
      case AutoEnrollmentState::kDisabled:
        uma_suffix = kUMASuffixDisabled;
        break;
      case AutoEnrollmentState::kEnrollment:
        uma_suffix = kUMASuffixEnrollment;
        break;
      case AutoEnrollmentState::kNoEnrollment:
        uma_suffix = kUMASuffixNoEnrollment;
        break;
      case AutoEnrollmentState::kServerError:
        uma_suffix = kUMASuffixServerError;
        break;
    }

    base::UmaHistogramMediumTimes(kUMAStateDeterminationTotalDuration,
                                  fetch_duration);
    base::UmaHistogramMediumTimes(
        base::StrCat({kUMAStateDeterminationTotalDurationByState, uma_suffix}),
        fetch_duration);
  }

  void ReportStepDurationAndResetTimer(base::StringPiece uma_step_suffix) {
    base::UmaHistogramTimes(
        base::StrCat({kUMAStateDeterminationStepDuration, uma_step_suffix}),
        base::TimeTicks::Now() - step_started_);
    step_started_ = base::TimeTicks::Now();
  }

  void ReportResult(AutoEnrollmentState state) {
    DCHECK(state != AutoEnrollmentState::kIdle);
    DCHECK(state != AutoEnrollmentState::kPending);
    ReportTotalDuration(base::TimeTicks::Now() - fetch_started_, state);
    std::move(report_result_).Run(state);
  }

  // Used to report an error or the determined enrollment state. In production
  // code, this will point to `AutoEnrollmentController::UpdateState`.
  base::OnceCallback<void(AutoEnrollmentState)> report_result_;

  // Time at which overall fetch or individual step has been started.
  base::TimeTicks fetch_started_;
  base::TimeTicks step_started_;

  // Used to store the initial enrollment state (if available) in a dict at
  // `prefs::kServerBackedDeviceState`.
  // Must not be nullptr for initial enrollment state determination.
  raw_ptr<PrefService, ExperimentalAsh> local_state_ = nullptr;

  DeviceIdentifiers device_identifiers_;
  SystemClock system_clock_;
  Ownership ownership_;
  EmbargoDate embargo_date_;
  StateKeys state_keys_;
  RlweOprf oprf_;
  RlweQuery query_;
  EnrollmentState state_;

  DeterminationContext context_;
  base::WeakPtrFactory<Sequence> weak_factory_{this};
};

void EnrollmentStateFetcherImpl::Start() {
  call_sequence_->Start();
}

}  // namespace

// static
std::unique_ptr<EnrollmentStateFetcher> EnrollmentStateFetcher::Create(
    base::OnceCallback<void(AutoEnrollmentState)> report_result,
    PrefService* local_state,
    RlweClientFactory rlwe_client_factory,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ash::SystemClockClient* system_clock_client,
    ServerBackedStateKeysBroker* state_key_broker,
    ash::DeviceSettingsService* device_settings_service) {
  return std::make_unique<EnrollmentStateFetcherImpl>(
      std::move(report_result), local_state, rlwe_client_factory,
      device_management_service, url_loader_factory, system_clock_client,
      state_key_broker, device_settings_service);
}

// static
void EnrollmentStateFetcher::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kEnrollmentPsmResult, -1);
  registry->RegisterTimePref(prefs::kEnrollmentPsmDeterminationTime,
                             base::Time());
}

EnrollmentStateFetcher::~EnrollmentStateFetcher() = default;

}  // namespace policy

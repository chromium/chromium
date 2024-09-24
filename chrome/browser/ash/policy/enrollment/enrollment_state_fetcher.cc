// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_token_provider.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
  const std::string rlz_brand_code_hex = base::HexEncode(rlz_brand_code);
  const std::string id = rlz_brand_code_hex + "/" + serial_number;
  // The PSM client library, which consumes this proto, will hash non-sensitive
  // identifier and truncate to a few bits before sending it to the server,
  // ensuring privacy. Hence, we can use the same identifier for both fields.
  rlwe_id.set_non_sensitive_id(id);
  rlwe_id.set_sensitive_id(id);

  return rlwe_id;
}

std::string_view AutoEnrollmentStateToUmaSuffix(AutoEnrollmentState state) {
  if (state.has_value()) {
    switch (state.value()) {
      case AutoEnrollmentResult::kEnrollment:
      case AutoEnrollmentResult::kSuggestedEnrollment:
        return kUMASuffixEnrollment;
      case AutoEnrollmentResult::kNoEnrollment:
        return kUMASuffixNoEnrollment;
      case AutoEnrollmentResult::kDisabled:
        return kUMASuffixDisabled;
    }
  }

  // TODO(b/309921228): Add more suffixes.
  return absl::visit(
      base::Overloaded{
          [](AutoEnrollmentSafeguardTimeoutError) {
            return kUMASuffixConnectionError;
          },
          [](AutoEnrollmentSystemClockSyncError) {
            return kUMASuffixConnectionError;
          },
          [](AutoEnrollmentStateKeysRetrievalError) {
            return kUMASuffixStateKeysRetrievalError;
          },
          [](const AutoEnrollmentDMServerError& error) {
            return error.network_error.has_value() ? kUMASuffixConnectionError
                                                   : kUMASuffixServerError;
          },
          [](AutoEnrollmentStateAvailabilityResponseError) {
            return kUMASuffixServerError;
          },
          [](AutoEnrollmentPsmError) { return kUMASuffixServerError; },
          [](AutoEnrollmentStateRetrievalResponseError) {
            return kUMASuffixServerError;
          },
      },
      state.error());
}

// The DeterminationContext is used to store state and cache computed values
// used at various steps of the enrollment state fetch sequence.
struct DeterminationContext {
  // Constructs client to communicate with PSM servers.
  // Must be set before sequence starts.
  EnrollmentStateFetcher::RlweClientFactory rlwe_client_factory;

  // Allows retrieving system values from multiple sources.
  // Must be set before sequence starts.
  raw_ptr<ash::system::StatisticsProvider> statistics_provider;

  // Interface for talking to DMServer.
  // Must be set before sequence starts.
  raw_ptr<DeviceManagementService> device_management_service = nullptr;

  // This will be used to configure `job`s for the `device_management_service`.
  // Must be set before sequence starts.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;

  // Used to retrieve device state keys.
  // Must be set before sequence starts.
  raw_ptr<ServerBackedStateKeysBroker> state_key_broker;

  // Interface for checking ownership.
  // Must be set before sequence starts.
  raw_ptr<ash::DeviceSettingsService> device_settings_service;

  // Enrollment token, included in state retrieval requests in order to obtain
  // enrollment-related data (e.g. license type) associated server-side with
  // the token. If this value is set, the device will make a state retrieval
  // request even if PSM returns false. As of writing, enrollment_token
  // is only included for Flex devices, to facilitate Flex Auto Enrollment.
  std::optional<std::string> enrollment_token;

  // RLZ brand code and serial numbers retrieved using `statistics_provider`.
  // Used for state availability determination (PSM) and state retrieval
  // requests. Computed by DeviceIdentifiers step.
  std::string rlz_brand_code;
  std::string serial_number;

  // State key retrieved from session_manager. Used for state retrieval
  // request. Computed by StateKeys step.
  std::optional<std::string> state_key;

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
  using Result = base::expected<Response, AutoEnrollmentError>;
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
          .Run(base::unexpected(AutoEnrollmentPsmError{}));
    }

    // Prepare the RLWE OPRF request job.
    auto config = std::make_unique<DMServerJobConfiguration>(
        context.device_management_service,
        DeviceManagementService::JobConfiguration::
            TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/std::nullopt, context.url_loader_factory,
        base::BindOnce(&RlweOprf::OnRequestDone, weak_factory_.GetWeakPtr(),
                       std::move(completion_callback)));

    *config->request()
         ->mutable_private_set_membership_request()
         ->mutable_rlwe_request()
         ->mutable_oprf_request() = *oprf_request;

    LOG(WARNING) << "Send PSM RLWE OPRF request";
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

    if (result.dm_status != DM_STATUS_SUCCESS) {
      const auto error =
          AutoEnrollmentDMServerError::FromDMServerJobResult(result);

      if (error.network_error.has_value()) {
        LOG(ERROR) << "PSM RLWE OPRF connection error";
      } else {
        LOG(ERROR) << "PSM RLWE OPRF server error: " << error.dm_error;
      }

      return std::move(completion_callback).Run(base::unexpected(error));
    }

    if (!result.response.has_private_set_membership_response() ||
        !result.response.private_set_membership_response()
             .has_rlwe_response() ||
        !result.response.private_set_membership_response()
             .rlwe_response()
             .has_oprf_response()) {
      LOG(ERROR) << "Empty PSM RLWE OPRF response";
      return std::move(completion_callback)
          .Run(
              base::unexpected(AutoEnrollmentStateAvailabilityResponseError{}));
    }

    // Handle success
    LOG(WARNING) << "PSM RLWE OPRF request completed successfully";
    return std::move(completion_callback)
        .Run(result.response.private_set_membership_response()
                 .rlwe_response()
                 .oprf_response());
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

  using Result = base::expected<bool, AutoEnrollmentError>;
  using CompletionCallback = base::OnceCallback<void(Result)>;

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
          .Run(base::unexpected(AutoEnrollmentPsmError{}));
    }

    // Prepare the RLWE query request job.
    auto config = std::make_unique<DMServerJobConfiguration>(
        context.device_management_service,
        DeviceManagementService::JobConfiguration::
            TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/std::nullopt, context.url_loader_factory,
        base::BindOnce(&RlweQuery::OnRequestDone, weak_factory_.GetWeakPtr(),
                       base::Unretained(context.psm_rlwe_client.get()),
                       std::move(completion_callback)));

    *config->request()
         ->mutable_private_set_membership_request()
         ->mutable_rlwe_request()
         ->mutable_query_request() = *query_request;

    LOG(WARNING) << "Send PSM RLWE query request";
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

    if (result.dm_status != DM_STATUS_SUCCESS) {
      const auto error =
          AutoEnrollmentDMServerError::FromDMServerJobResult(result);

      if (error.network_error.has_value()) {
        LOG(ERROR) << "PSM RLWE query connection error";
      } else {
        LOG(ERROR) << "PSM RLWE query server error: " << error.dm_error;
      }

      return std::move(completion_callback).Run(base::unexpected(error));
    }

    // Check if the RLWE query response is empty.
    if (!result.response.has_private_set_membership_response() ||
        !result.response.private_set_membership_response()
             .has_rlwe_response() ||
        !result.response.private_set_membership_response()
             .rlwe_response()
             .has_query_response()) {
      LOG(ERROR) << "Empty PSM RLWE query response";

      return std::move(completion_callback)
          .Run(
              base::unexpected(AutoEnrollmentStateAvailabilityResponseError{}));
    }

    const auto responses = psm_rlwe_client->ProcessQueryResponse(
        result.response.private_set_membership_response()
            .rlwe_response()
            .query_response());

    if (!responses.ok() || responses->membership_responses_size() != 1) {
      LOG(ERROR) << "Invalid PSM RLWE query response";

      return std::move(completion_callback)
          .Run(
              base::unexpected(AutoEnrollmentStateAvailabilityResponseError{}));
    }

    if (responses->membership_responses_size() != 1) {
      LOG(ERROR) << "Invalid PSM RLWE query response: "
                 << responses->membership_responses_size()
                 << " membership responses, expected 1";

      return std::move(completion_callback)
          .Run(
              base::unexpected(AutoEnrollmentStateAvailabilityResponseError{}));
    }

    const bool is_member =
        responses->membership_responses(0).membership_response().is_member();
    return std::move(completion_callback).Run(is_member);
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

  void MarkResultIgnoredForTokenBasedEnrollment(PrefService* local_state) {
    local_state->SetTime(prefs::kEnrollmentPsmDeterminationTime,
                         base::Time::Now());
    // TODO(b/331285209): Consider changing name of
    // PSM_SKIPPED_FOR_FLEX_AUTO_ENROLLMENT (unlikely since it's in a shared
    // proto), or adding a new value, to remove "Flex" from the name, and
    // change "skipped" to "ignored", as "skipped" isn't entirely accurate here.
    local_state->SetInteger(
        prefs::kEnrollmentPsmResult,
        em::DeviceRegisterRequest::PSM_SKIPPED_FOR_FLEX_AUTO_ENROLLMENT);
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

  using CompletionCallback = base::OnceCallback<void(
      base::expected<std::optional<std::string>,
                     ServerBackedStateKeysBroker::ErrorType>)>;

  // If FRE is enabled, this will try up to `kMaxAttempts` times to obtain the
  // state keys. If successful, it will call the completion callback with the
  // current state key as the callback's expected value. If unsuccessful, the
  // callback will be passed an unexpected (error) value.
  //
  // If FRE is not enabled, the completion callback will be called with the
  // `std::nullopt` as the callback's expected value.
  void Retrieve(ServerBackedStateKeysBroker* state_key_broker,
                CompletionCallback completion_callback) {
    ++attempts_;
    LOG(WARNING) << "Requesting state keys. Attempt " << attempts_ << ".";
    state_key_broker->RequestStateKeys(base::BindOnce(
        &StateKeys::OnStateKeysRetrieved, weak_factory_.GetWeakPtr(),
        state_key_broker, std::move(completion_callback)));
  }

 private:
  void OnStateKeysRetrieved(ServerBackedStateKeysBroker* state_key_broker,
                            CompletionCallback completion_callback,
                            const std::vector<std::string>& state_keys) {
    const auto error_type = state_key_broker->error_type();
    if (error_type == ServerBackedStateKeysBroker::ErrorType::kNoError) {
      CHECK(!state_keys.empty());  // This is guaranteed by the broker.
      return std::move(completion_callback).Run(state_keys.front());
    }
    if (attempts_ >= kMaxAttempts) {
      return std::move(completion_callback).Run(base::unexpected(error_type));
    }
    return Retrieve(state_key_broker, std::move(completion_callback));
  }

  int attempts_ = 0;
  base::WeakPtrFactory<StateKeys> weak_factory_{this};
};

// Class to send state request to DMServer.
//
// This is a step in enrollment state fetch (see Sequence class below).
class EnrollmentState {
 public:
  struct Result {
    base::Value::Dict dict;
    AutoEnrollmentState state;
  };
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
        /*oauth_token=*/std::nullopt, context.url_loader_factory,
        base::BindOnce(&EnrollmentState::OnRequestDone,
                       weak_factory_.GetWeakPtr(),
                       std::move(completion_callback)));

    auto* request = config->request()->mutable_device_state_retrieval_request();
    if (context.state_key.has_value()) {
      request->set_server_backed_state_key(context.state_key.value());
    }
    if (context.enrollment_token.has_value()) {
      LOG(WARNING) << "Setting enrollment token on DeviceStateRetrievalRequest";
      request->set_enrollment_token(context.enrollment_token.value());
    }
    request->set_brand_code(std::string(context.rlz_brand_code));
    request->set_serial_number(std::string(context.serial_number));

    LOG(WARNING) << "Send unified enrollment state retrieval request";
    job_ = context.device_management_service->CreateJob(std::move(config));
  }

  void OnRequestDone(CompletionCallback completion_callback,
                     DMServerJobResult result) {
    // Handle errors
    base::UmaHistogramSparse(kUMAStateDeterminationStateRequestDmStatusCode,
                             result.dm_status);
    base::UmaHistogramSparse(kUMAStateDeterminationStateRequestNetworkErrorCode,
                             -result.net_error);
    if (result.dm_status != DM_STATUS_SUCCESS) {
      const auto error =
          AutoEnrollmentDMServerError::FromDMServerJobResult(result);

      if (error.network_error.has_value()) {
        LOG(ERROR) << "Enrollment state query connection error";
      } else {
        LOG(ERROR) << "Enrollment state query server error";
      }

      return std::move(completion_callback)
          .Run(Result{.state = base::unexpected(error)});
    }

    if (!result.response.has_device_state_retrieval_response()) {
      LOG(ERROR) << "Server failed to provide unified enrollment response.";
      return std::move(completion_callback)
          .Run(Result{.state = base::unexpected(
                          AutoEnrollmentStateRetrievalResponseError{})});
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
    Result result;
    std::string mode;
    std::tie(result.state, mode) =
        ConvertInitialEnrollmentMode(state_response.initial_enrollment_mode());
    if (!mode.empty()) {
      result.dict.Set(kDeviceStateMode, mode);
    }

    if (state_response.has_management_domain()) {
      result.dict.Set(kDeviceStateManagementDomain,
                      state_response.management_domain());
    }

    if (state_response.has_is_license_packaged_with_device()) {
      result.dict.Set(kDeviceStatePackagedLicense,
                      state_response.is_license_packaged_with_device());
    }

    if (state_response.has_license_packaging_sku()) {
      result.dict.Set(
          kDeviceStateLicenseType,
          ConvertLicenseType(state_response.license_packaging_sku()));
    }

    if (state_response.has_assigned_upgrade_type()) {
      result.dict.Set(
          kDeviceStateAssignedUpgradeType,
          ConvertAssignedUpgradeType(state_response.assigned_upgrade_type()));
    }

    if (state_response.has_disabled_state()) {
      result.dict.Set(kDeviceStateDisabledMessage,
                      state_response.disabled_state().message());
    }

    LOG(WARNING) << "Initial enrollment mode = '" << mode << "', "
                 << (state_response.is_license_packaged_with_device() ? "with"
                                                                      : "no")
                 << " packaged license.";

    base::UmaHistogramBoolean(
        base::StrCat({kUMAStateDeterminationIsInitialByState,
                      AutoEnrollmentStateToUmaSuffix(result.state)}),
        true);
    return std::move(completion_callback).Run(std::move(result));
  }

  void ParseSecondaryStateResponse(
      const em::DeviceStateRetrievalResponse& state_response,
      CompletionCallback completion_callback) {
    Result result;
    std::string mode;
    std::tie(result.state, mode) =
        ConvertRestoreMode(state_response.restore_mode());
    if (!mode.empty()) {
      result.dict.Set(kDeviceStateMode, mode);
    }

    if (state_response.has_management_domain()) {
      result.dict.Set(kDeviceStateManagementDomain,
                      state_response.management_domain());
    }

    if (state_response.has_disabled_state()) {
      result.dict.Set(kDeviceStateDisabledMessage,
                      state_response.disabled_state().message());
    }

    if (state_response.has_license_type()) {
      result.dict.Set(kDeviceStateLicenseType,
                      ConvertAutoEnrollmentLicenseType(
                          state_response.license_type().license_type()));
    }

    LOG(WARNING) << "Received restore mode " << mode;
    base::UmaHistogramBoolean(
        base::StrCat({kUMAStateDeterminationIsInitialByState,
                      AutoEnrollmentStateToUmaSuffix(result.state)}),
        false);
    return std::move(completion_callback).Run(std::move(result));
  }

  void StoreResponse(PrefService* local_state, const base::Value::Dict& dict) {
    LOG(WARNING) << "ServerBackedDeviceState pref: " << dict;
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
        return {AutoEnrollmentResult::kNoEnrollment, std::string()};
      case Response::INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED:
        return {AutoEnrollmentResult::kEnrollment,
                kDeviceStateInitialModeEnrollmentEnforced};
      case Response::INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED:
        return {AutoEnrollmentResult::kEnrollment,
                kDeviceStateInitialModeEnrollmentZeroTouch};
      case Response::INITIAL_ENROLLMENT_MODE_DISABLED:
        return {AutoEnrollmentResult::kDisabled, kDeviceStateModeDisabled};
      case Response::INITIAL_ENROLLMENT_MODE_TOKEN_ENROLLMENT_ENFORCED:
        return {AutoEnrollmentResult::kEnrollment,
                kDeviceStateInitialModeTokenEnrollment};
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
        return {AutoEnrollmentResult::kNoEnrollment, std::string()};
      case em::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED:
        return {AutoEnrollmentResult::kEnrollment,
                kDeviceStateRestoreModeReEnrollmentRequested};
      case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED:
        return {AutoEnrollmentResult::kEnrollment,
                kDeviceStateRestoreModeReEnrollmentEnforced};
      case em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED:
        return {AutoEnrollmentResult::kDisabled, kDeviceStateModeDisabled};
      case em::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
        return {AutoEnrollmentResult::kEnrollment,
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
      ServerBackedStateKeysBroker* state_key_broker,
      ash::DeviceSettingsService* device_settings_service,
      ash::OobeConfiguration* oobe_configuration) {
    DCHECK(report_result);
    DCHECK(local_state);
    DCHECK(rlwe_client_factory);
    DCHECK(device_management_service);
    DCHECK(url_loader_factory);
    DCHECK(state_key_broker);
    DCHECK(device_settings_service);
    DCHECK(oobe_configuration);

    call_sequence_ = std::make_unique<Sequence>(
        std::move(report_result), local_state,
        DeterminationContext{std::move(rlwe_client_factory),
                             ash::system::StatisticsProvider::GetInstance(),
                             device_management_service, url_loader_factory,
                             state_key_broker, device_settings_service,
                             GetEnrollmentToken(oobe_configuration)});
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
      LOG(WARNING) << "Unified state determination is disabled";
      return ReportResult(AutoEnrollmentResult::kNoEnrollment);
    }

    // Report whether we're doing FRE on Flex or not.
    base::UmaHistogramBoolean(kUMAStateDeterminationOnFlex,
                              ash::switches::IsRevenBranding());

    // TODO(b/265923216): Investigate the possibility of using bypassing PSM and
    // using state key to directly request state when identifiers are missing.
    if (!device_identifiers_.Retrieve(context_.statistics_provider,
                                      context_.rlz_brand_code,
                                      context_.serial_number)) {
      // Skip enrollment if serial number or brand code are missing.
      return ReportResult(AutoEnrollmentResult::kNoEnrollment);
    }

    step_started_ = base::TimeTicks::Now();
    ownership_.Check(context_.device_settings_service,
                     base::BindOnce(&Sequence::OnOwnershipChecked,
                                    weak_factory_.GetWeakPtr()));
  }

 private:
  void OnOwnershipChecked(ash::DeviceSettingsService::OwnershipStatus status) {
    ReportStepDurationAndResetTimer(kUMASuffixOwnershipCheck);
    base::UmaHistogramEnumeration(kUMAStateDeterminationOwnershipStatus,
                                  status);
    if (status ==
        ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown) {
      LOG(ERROR) << "Device ownership is unknown. Skipping enrollment";
      return ReportResult(AutoEnrollmentResult::kNoEnrollment);
    }

    if (status ==
        ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
      LOG(WARNING) << "Device ownership is already taken. Skipping enrollment";
      return ReportResult(AutoEnrollmentResult::kNoEnrollment);
    }

    oprf_.Request(context_, base::BindOnce(&Sequence::OnOprfRequestDone,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnOprfRequestDone(RlweOprf::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixOPRFRequest);
    if (!result.has_value()) {
      StorePsmError(local_state_);
      if (absl::holds_alternative<AutoEnrollmentPsmError>(result.error())) {
        return ReportResult(AutoEnrollmentResult::kNoEnrollment);
      }

      return ReportResult(base::unexpected(result.error()));
    }
    query_.Request(context_, result.value(),
                   base::BindOnce(&Sequence::OnQueryRequestDone,
                                  weak_factory_.GetWeakPtr()));
  }

  void OnQueryRequestDone(RlweQuery::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixQueryRequest);

    if (!result.has_value()) {
      StorePsmError(local_state_);
      if (absl::holds_alternative<AutoEnrollmentPsmError>(result.error())) {
        return ReportResult(AutoEnrollmentResult::kNoEnrollment);
      }

      return ReportResult(base::unexpected(result.error()));
    }

    RlwePlaintextId psm_id =
        ConstructPlainttextId(context_.rlz_brand_code, context_.serial_number);
    // Use WARNING level to preserve PSM ID in the logs.
    LOG(WARNING) << "PSM determination successful. Identifier "
                 << psm_id.sensitive_id() << " is"
                 << (result.value() ? "" : " not") << " present on the server";

    base::UmaHistogramBoolean(kUMAStateDeterminationPsmReportedAvailableState,
                              result.value());

    if (!result.value()) {
      if (context_.enrollment_token.has_value()) {
        query_.MarkResultIgnoredForTokenBasedEnrollment(local_state_);
      } else {
        // There is no PSM record nor enrollment token present, device doesn't
        // need to proceed to further steps.
        return ReportResult(AutoEnrollmentResult::kNoEnrollment);
      }
    } else {
      query_.StoreResponse(local_state_, result.value());
    }

    if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
      state_keys_.Retrieve(context_.state_key_broker,
                           base::BindOnce(&Sequence::OnStateKeyRetrieved,
                                          weak_factory_.GetWeakPtr()));
    } else {
      LOG(WARNING) << "Forced re-enrollment is not enabled. No need to "
                      "retrieve a re-enrollment (a.k.a. state) key.";
      OnStateKeyRetrieved(std::nullopt);
    }
  }

  void OnStateKeyRetrieved(
      base::expected<std::optional<std::string>,
                     ServerBackedStateKeysBroker::ErrorType> state_key) {
    ReportStepDurationAndResetTimer(kUMASuffixStateKeysRetrieval);
    base::UmaHistogramEnumeration(
        kUMAStateDeterminationStateKeysRetrievalErrorType,
        state_key.error_or(ServerBackedStateKeysBroker::ErrorType::kNoError));
    if (state_key.has_value()) {
      context_.state_key = state_key.value();
    } else {
      switch (state_key.error()) {
        case ServerBackedStateKeysBroker::ErrorType::kMissingIdentifiers:
          // Missing identifiers is typically a permanent error, hence we
          // proceed to attempt state retrieval with just serial number
          // and brand code.
          LOG(WARNING)
              << "Failed to obtain state keys due to missing identifiers";
          context_.state_key.reset();
          break;
        case ServerBackedStateKeysBroker::ErrorType::kCommunicationError:
        case ServerBackedStateKeysBroker::ErrorType::kInvalidResponse:
          LOG(ERROR) << "Failed to obtain state keys. Error: "
                     << static_cast<int>(state_key.error());
          // These errors are typically transient, hence we block here to
          // enforce a retry and avoid potential FRE escapes.
          return ReportResult(
              base::unexpected(AutoEnrollmentStateKeysRetrievalError{}));
        case ServerBackedStateKeysBroker::ErrorType::kNoError:
          NOTREACHED_IN_MIGRATION();
      }
    }
    state_.Request(context_, base::BindOnce(&Sequence::OnStateRequestDone,
                                            weak_factory_.GetWeakPtr()));
  }

  void OnStateRequestDone(EnrollmentState::Result result) {
    ReportStepDurationAndResetTimer(kUMASuffixStateRequest);
    base::UmaHistogramBoolean(kUMAStateDeterminationStateReturned,
                              result.state.has_value());
    if (result.state.has_value()) {
      state_.StoreResponse(local_state_, result.dict);
    }

    return ReportResult(result.state);
  }

  // Helpers
  void ReportTotalDuration(base::TimeDelta fetch_duration,
                           AutoEnrollmentState state) {
    const std::string_view uma_suffix = AutoEnrollmentStateToUmaSuffix(state);

    base::UmaHistogramMediumTimes(kUMAStateDeterminationTotalDuration,
                                  fetch_duration);
    base::UmaHistogramMediumTimes(
        base::StrCat({kUMAStateDeterminationTotalDurationByState, uma_suffix}),
        fetch_duration);
  }

  void ReportStepDurationAndResetTimer(std::string_view uma_step_suffix) {
    base::UmaHistogramTimes(
        base::StrCat({kUMAStateDeterminationStepDuration, uma_step_suffix}),
        base::TimeTicks::Now() - step_started_);
    step_started_ = base::TimeTicks::Now();
  }

  void ReportResult(AutoEnrollmentState state) {
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
  raw_ptr<PrefService> local_state_ = nullptr;

  DeviceIdentifiers device_identifiers_;
  Ownership ownership_;
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
    ServerBackedStateKeysBroker* state_key_broker,
    ash::DeviceSettingsService* device_settings_service,
    ash::OobeConfiguration* oobe_configuration) {
  return std::make_unique<EnrollmentStateFetcherImpl>(
      std::move(report_result), local_state, rlwe_client_factory,
      device_management_service, url_loader_factory, state_key_broker,
      device_settings_service, oobe_configuration);
}

// static
void EnrollmentStateFetcher::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kEnrollmentPsmResult, -1);
  registry->RegisterTimePref(prefs::kEnrollmentPsmDeterminationTime,
                             base::Time());
}

EnrollmentStateFetcher::~EnrollmentStateFetcher() = default;

}  // namespace policy

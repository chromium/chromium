// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/fresnel_pref_names.h"
#include "ash/components/device_activity/fresnel_service.pb.h"
#include "base/i18n/time_formatting.h"
// TODO(https://crbug.com/1269900): Migrate to use SFUL library.
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "crypto/hmac.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash {
namespace device_activity {
namespace {

// Amount of time to wait before retriggering repeating timer.
constexpr base::TimeDelta kTimeToRepeat = base::Hours(1);

// General upper bound of expected Fresnel response size in bytes.
constexpr size_t kMaxFresnelResponseSizeBytes = 1 << 20;  // 1MB;

// Timeout for each Fresnel request.
constexpr base::TimeDelta kHealthCheckRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kImportRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kOprfRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kQueryRequestTimeout = base::Seconds(60);

// TODO(https://crbug.com/1272922): Move shared configuration constants to
// separate file.
const char kFresnelHealthCheckEndpoint[] = "/v1/fresnel/healthCheck";
const char kFresnelImportRequestEndpoint[] = "/v1/fresnel/psmRlweImport";
const char kFresnelOprfRequestEndpoint[] = "/v1/fresnel/psmRlweOprf";
const char kFresnelQueryRequestEndpoint[] = "/v1/fresnel/psmRlweQuery";

// UMA histograms defined in:
// //tools/metrics/histograms/metadata/ash/histograms.xml.
//
// Count number of times a state has been entered.
const char kHistogramStateCount[] = "Ash.DeviceActiveClient.StateCount";

// Duration histogram uses State variant in order to create
// unique histograms measuring durations by State.
const char kHistogramDurationPrefix[] = "Ash.DeviceActiveClient.Duration";

// Response histogram uses State variant in order to create
// unique histograms measuring responses by State.
const char kHistogramResponsePrefix[] = "Ash.DeviceActiveClient.Response";

// Count the number of boolean membership request results.
const char kDeviceActiveClientQueryMembershipResult[] =
    "Ash.DeviceActiveClient.QueryMembershipResult";

// Generates the full histogram name for histogram variants based on state.
std::string HistogramVariantName(const std::string& histogram_prefix,
                                 DeviceActivityClient::State state) {
  switch (state) {
    case DeviceActivityClient::State::kIdle:
      return base::StrCat({histogram_prefix, ".Idle"});
    case DeviceActivityClient::State::kCheckingMembershipOprf:
      return base::StrCat({histogram_prefix, ".CheckingMembershipOprf"});
    case DeviceActivityClient::State::kCheckingMembershipQuery:
      return base::StrCat({histogram_prefix, ".CheckingMembershipQuery"});
    case DeviceActivityClient::State::kCheckingIn:
      return base::StrCat({histogram_prefix, ".CheckingIn"});
    case DeviceActivityClient::State::kHealthCheck:
      return base::StrCat({histogram_prefix, ".HealthCheck"});
    default:
      NOTREACHED() << "Invalid State.";
      return base::StrCat({histogram_prefix, ".Unknown"});
  }
}

void RecordStateCountMetric(DeviceActivityClient::State state) {
  base::UmaHistogramEnumeration(kHistogramStateCount, state);
}

void RecordQueryMembershipResultBoolean(bool is_member) {
  base::UmaHistogramBoolean(kDeviceActiveClientQueryMembershipResult,
                            is_member);
}

// Histogram sliced by duration and state.
void RecordDurationStateMetric(DeviceActivityClient::State state,
                               const base::TimeDelta duration) {
  std::string duration_state_histogram_name =
      HistogramVariantName(kHistogramDurationPrefix, state);
  base::UmaHistogramCustomTimes(duration_state_histogram_name, duration,
                                base::Milliseconds(1), base::Seconds(100),
                                100 /* number of histogram buckets */);
}

// Histogram slices by PSM response and state.
void RecordResponseStateMetric(DeviceActivityClient::State state,
                               int net_code) {
  // Mapping status code to PsmResponse is used to record UMA histograms
  // for responses by state.
  DeviceActivityClient::PsmResponse response;
  switch (net_code) {
    case net::OK:
      response = DeviceActivityClient::PsmResponse::kSuccess;
      break;
    case net::ERR_TIMED_OUT:
      response = DeviceActivityClient::PsmResponse::kTimeout;
      break;
    default:
      response = DeviceActivityClient::PsmResponse::kError;
      break;
  }

  base::UmaHistogramEnumeration(
      HistogramVariantName(kHistogramResponsePrefix, state), response);
}

std::unique_ptr<network::ResourceRequest> GenerateResourceRequest(
    const std::string& request_method,
    const GURL& url,
    const std::string& api_key) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = request_method;
  resource_request->headers.SetHeader("x-goog-api-key", api_key);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/x-protobuf");

  return resource_request;
}

// TODO(https://crbug.com/1262177): currently the PSM use cases are not synced
// with google3. Update to retrieve from synced RlweUseCase in file:
// third_party/private_membership/src/private_membership_rlwe.proto.
constexpr psm_rlwe::RlweUseCase kDailyPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY;

// Generate the window identifier for the kCrosDaily use case.
// For example, the daily use case should generate a window identifier
// formatted: yyyyMMdd.
// TODO(https://crbug.com/1262187): This window identifier will need to support
// more use cases in the future. Currently it only supports the kCrosDaily use
// case.
std::string GenerateUTCWindowIdentifier(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d%02d", exploded.year, exploded.month,
                            exploded.day_of_month);
}

// Calculates an HMAC of |message| using |key|, encoded as a hexadecimal string.
// Return empty string if HMAC fails.
std::string GetDigestString(const std::string& key,
                            const std::string& message) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::vector<uint8_t> digest(hmac.DigestLength());
  if (!hmac.Init(key) || !hmac.Sign(message, &digest[0], digest.size())) {
    return std::string();
  }
  return base::HexEncode(&digest[0], digest.size());
}

// Generate the PSM identifier, used to identify a fixed
// window of time for device active counting. Privacy compliance is guaranteed
// by retrieving the |psm_device_active_secret_| from chromeos, and
// performing an additional HMAC-SHA256 hash on generated plaintext string.
absl::optional<psm_rlwe::RlwePlaintextId> GeneratePsmIdentifier(
    const std::string& psm_device_active_secret,
    const std::string& psm_use_case,
    const std::string& window_id) {
  if (psm_device_active_secret.empty() || psm_use_case.empty() ||
      window_id.empty())
    return absl::nullopt;

  std::string unhashed_psm_id =
      base::JoinString({psm_use_case, window_id}, "|");

  // Convert bytes to hex to avoid encoding/decoding proto issues across
  // client/server.
  std::string psm_id_hex =
      GetDigestString(psm_device_active_secret, unhashed_psm_id);

  if (!psm_id_hex.empty()) {
    psm_rlwe::RlwePlaintextId psm_rlwe_id;
    psm_rlwe_id.set_sensitive_id(psm_id_hex);
    return psm_rlwe_id;
  }

  // Failed HMAC-SHA256 hash on PSM id.
  return absl::nullopt;
}

// Determines if |prev_ping_ts| occurred in a different daily active window then
// |new_ping_ts| for a given device. Performing this check helps reduce QPS to
// the |CheckingMembership| network requests.
// TODO(https://crbug.com/1262187): This function will need to get modified to
// support kCrosMonthly and kCrosAllTime use cases.
bool IsDailyDeviceActivePingRequired(base::Time prev_ping_ts,
                                     base::Time new_ping_ts) {
  std::string prev_ping_ts_period = GenerateUTCWindowIdentifier(prev_ping_ts);
  std::string new_ping_ts_period = GenerateUTCWindowIdentifier(new_ping_ts);

  return prev_ping_ts < new_ping_ts &&
         prev_ping_ts_period != new_ping_ts_period;
}

}  // namespace

DeviceActivityClient::DeviceActivityClient(
    NetworkStateHandler* handler,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<PsmDelegate> psm_delegate,
    std::unique_ptr<base::RepeatingTimer> report_timer,
    const std::string& fresnel_base_url,
    const std::string& api_key,
    const std::string& psm_device_active_secret)
    : network_state_handler_(handler),
      local_state_(local_state),
      url_loader_factory_(url_loader_factory),
      psm_delegate_(std::move(psm_delegate)),
      report_timer_(std::move(report_timer)),
      fresnel_base_url_(fresnel_base_url),
      api_key_(api_key),
      psm_device_active_secret_(psm_device_active_secret) {
  DCHECK(network_state_handler_);
  DCHECK(local_state_);
  DCHECK(url_loader_factory_);
  DCHECK(psm_delegate_);
  DCHECK(report_timer_);

  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &DeviceActivityClient::TransitionOutOfIdle);

  network_state_handler_->AddObserver(this, FROM_HERE);
  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

DeviceActivityClient::~DeviceActivityClient() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

base::RepeatingTimer* DeviceActivityClient::GetReportTimer() {
  return report_timer_.get();
}

// Method gets called when the state of the default (primary)
// network OR properties of the default network changes.
void DeviceActivityClient::DefaultNetworkChanged(const NetworkState* network) {
  bool was_connected = network_connected_;
  network_connected_ = network && network->IsOnline();

  if (network_connected_ == was_connected)
    return;
  if (network_connected_)
    OnNetworkOnline();
}

DeviceActivityClient::State DeviceActivityClient::GetState() const {
  return state_;
}

void DeviceActivityClient::OnNetworkOnline() {
  TransitionOutOfIdle();
}

GURL DeviceActivityClient::GetFresnelURL() const {
  GURL base_url(fresnel_base_url_);
  GURL::Replacements replacements;

  switch (state_) {
    case State::kCheckingMembershipOprf:
      replacements.SetPathStr(kFresnelOprfRequestEndpoint);
      break;
    case State::kCheckingMembershipQuery:
      replacements.SetPathStr(kFresnelQueryRequestEndpoint);
      break;
    case State::kCheckingIn:
      replacements.SetPathStr(kFresnelImportRequestEndpoint);
      break;
    case State::kHealthCheck:
      replacements.SetPathStr(kFresnelHealthCheckEndpoint);
      break;
    case State::kIdle:  // Fallthrough to |kUnknown| case.
      FALLTHROUGH;
    case State::kUnknown:
      NOTREACHED();
      break;
  }

  return base_url.ReplaceComponents(replacements);
}

void DeviceActivityClient::InitializeDeviceMetadata(
    DeviceMetadata* device_metadata) {
  device_metadata->set_chromeos_version(version_info::GetMajorVersionNumber());
}

// TODO(https://crbug.com/1262189): Add callback to report actives only after
// synchronizing the system clock.
void DeviceActivityClient::TransitionOutOfIdle() {
  if (!network_connected_ || state_ != State::kIdle) {
    TransitionToIdle();
    return;
  }

  // Check the last recorded daily ping timestamp in local state prefs.
  // This variable has the default Unix Epoch value if the device is
  // new, powerwashed, recovered, or a RMA device.
  base::Time last_recorded_daily_ping_time =
      local_state_->GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  // The network is connected and the client |state_| is kIdle.
  last_transition_out_of_idle_time_ = base::Time::Now();

  // Begin phase one of checking membership if the device has not pinged yet
  // within the given use case window.
  // TODO(https://crbug.com/1262187): Remove hardcoded use case when adding
  // support for additional use cases (i.e MONTHLY, ALL_TIME, etc.).
  if (IsDailyDeviceActivePingRequired(last_recorded_daily_ping_time,
                                      last_transition_out_of_idle_time_)) {
    current_day_window_id_ =
        GenerateUTCWindowIdentifier(last_transition_out_of_idle_time_);
    current_day_psm_id_ = GeneratePsmIdentifier(
        psm_device_active_secret_, psm_rlwe::RlweUseCase_Name(kDailyPsmUseCase),
        current_day_window_id_.value());

    // Check if the PSM id is generated.
    if (!current_day_psm_id_.has_value()) {
      TransitionToIdle();
      return;
    }

    std::vector<psm_rlwe::RlwePlaintextId> psm_rlwe_ids = {
        current_day_psm_id_.value()};
    auto status_or_client = psm_delegate_->CreatePsmClient(
        psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY, psm_rlwe_ids);

    if (!status_or_client.ok()) {
      TransitionToIdle();
      return;
    }
    psm_rlwe_client_ = std::move(status_or_client.value());

    // During rollout, we perform CheckIn without CheckMembership for powerwash,
    // recovery, or RMA devices.
    TransitionToCheckIn();
  }
}

void DeviceActivityClient::TransitionToHealthCheck() {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kHealthCheck;

  // Report UMA histogram for transitioning state to |kHealthCheck|.
  RecordStateCountMetric(state_);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kGetMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);

  url_loader_->SetTimeoutDuration(kHealthCheckRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnHealthCheckDone,
                     weak_factory_.GetWeakPtr()),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnHealthCheckDone(
    std::unique_ptr<std::string> response_body) {
  DCHECK_EQ(state_, State::kHealthCheck);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Record duration of |kHealthCheck| state.
  RecordDurationStateMetric(state_, state_timer_.Elapsed());

  // Transition back to kIdle state after performing a health check on servers.
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToCheckMembershipOprf() {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipOprf;

  // Report UMA histogram for transitioning state to |kCheckingMembershipOprf|.
  RecordStateCountMetric(state_);

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request = psm_rlwe_client_->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfRequest oprf_request =
      status_or_oprf_request.value();

  // Wrap PSM Oprf request body by FresnelPsmRlweOprfRequest proto.
  // This proto is expected by the Fresnel service.
  device_activity::FresnelPsmRlweOprfRequest fresnel_oprf_request;
  *fresnel_oprf_request.mutable_rlwe_oprf_request() = oprf_request;

  std::string request_body;
  fresnel_oprf_request.SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kOprfRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckMembershipOprfDone,
                     weak_factory_.GetWeakPtr()),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipOprfDone(
    std::unique_ptr<std::string> response_body) {
  DCHECK_EQ(state_, State::kCheckingMembershipOprf);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Convert serialized response body to oprf response protobuf.
  FresnelPsmRlweOprfResponse psm_oprf_response;
  if (!response_body || !psm_oprf_response.ParseFromString(*response_body)) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  // Parse |fresnel_oprf_response| for oprf_response.
  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToCheckMembershipQuery(oprf_response);
}

void DeviceActivityClient::TransitionToCheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  DCHECK_EQ(state_, State::kCheckingMembershipOprf);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipQuery;

  // Report UMA histogram for transitioning state to |kCheckingMembershipQuery|.
  RecordStateCountMetric(state_);

  // Generate PSM Query request body.
  const auto status_or_query_request =
      psm_rlwe_client_->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryRequest query_request =
      status_or_query_request.value();

  // Wrap PSM Query request body by FresnelPsmRlweQueryRequest proto.
  // This proto is expected by the Fresnel service.
  device_activity::FresnelPsmRlweQueryRequest fresnel_query_request;
  *fresnel_query_request.mutable_rlwe_query_request() = query_request;

  std::string request_body;
  fresnel_query_request.SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kQueryRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckMembershipQueryDone,
                     weak_factory_.GetWeakPtr()),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipQueryDone(
    std::unique_ptr<std::string> response_body) {
  DCHECK_EQ(state_, State::kCheckingMembershipQuery);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Convert serialized response body to fresnel query response protobuf.
  FresnelPsmRlweQueryResponse psm_query_response;
  if (!response_body || !psm_query_response.ParseFromString(*response_body)) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  // Parse |fresnel_query_response| for psm query_response.
  if (!psm_query_response.has_rlwe_query_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();

  auto status_or_response = psm_rlwe_client_->ProcessResponse(query_response);
  if (!status_or_response.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
    return;
  }

  psm_rlwe::MembershipResponseMap membership_response_map =
      status_or_response.value();
  private_membership::MembershipResponse membership_response =
      membership_response_map.Get(current_day_psm_id_.value());

  bool is_psm_id_member = membership_response.is_member();

  // Record the query membership result to UMA histogram.
  RecordQueryMembershipResultBoolean(is_psm_id_member);

  if (!is_psm_id_member) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToCheckIn();
  } else {
    // Update local state to signal ping has already been sent for current day.
    local_state_->SetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                          last_transition_out_of_idle_time_);
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle();
  }
}

void DeviceActivityClient::TransitionToCheckIn() {
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingIn;

  // Report UMA histogram for transitioning state to |kCheckingIn|.
  RecordStateCountMetric(state_);

  std::string current_psm_id_str = current_day_psm_id_.value().sensitive_id();

  // Generate Fresnel PSM import request body.
  device_activity::ImportDataRequest import_request;
  import_request.set_window_identifier(current_day_window_id_.value());
  import_request.set_plaintext_identifier(current_psm_id_str);
  import_request.set_use_case(kDailyPsmUseCase);

  // Important: Each new dimension added to metadata will need to be approved by
  // privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  InitializeDeviceMetadata(device_metadata);

  std::string request_body;
  import_request.SerializeToString(&request_body);

  auto resource_request = GenerateResourceRequest(
      net::HttpRequestHeaders::kPostMethod, GetFresnelURL(), api_key_);

  // TODO(https://crbug.com/1266972): Refactor |url_loader_| network request
  // call to a shared helper method.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader_->SetTimeoutDuration(kImportRequestTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DeviceActivityClient::OnCheckInDone,
                     weak_factory_.GetWeakPtr()),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckInDone(
    std::unique_ptr<std::string> response_body) {
  DCHECK_EQ(state_, State::kCheckingIn);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Successful import request - PSM ID was imported successfully.
  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    local_state_->SetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                          last_transition_out_of_idle_time_);
  }

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToIdle() {
  DCHECK(!url_loader_);
  state_ = State::kIdle;

  current_day_window_id_ = absl::nullopt;
  current_day_psm_id_ = absl::nullopt;

  // Report UMA histogram for transitioning state back to |kIdle|.
  RecordStateCountMetric(state_);
}

}  // namespace device_activity
}  // namespace ash

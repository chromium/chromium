// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/device_active_use_case.h"
#include "ash/components/device_activity/fresnel_service.pb.h"
#include "ash/constants/ash_features.h"
#include "base/check.h"
// TODO(https://crbug.com/1269900): Migrate to use SFUL library.
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

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

// Record the minute the device activity client transitions out of idle.
const char kDeviceActiveClientTransitionOutOfIdleMinute[] =
    "Ash.DeviceActiveClient.TransitionOutOfIdleMinute";

// Record the minute the device activity client transitions to check in.
const char kDeviceActiveClientTransitionToCheckInMinute[] =
    "Ash.DeviceActiveClient.TransitionToCheckInMinute";

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

// Return the minute of the current UTC time.
base::TimeDelta GetCurrentMinute() {
  base::Time cur_time = base::Time::Now();

  // Extract minute from exploded |cur_time| in UTC.
  base::Time::Exploded exploded_utc;
  cur_time.UTCExplode(&exploded_utc);

  return base::Minutes(exploded_utc.minute);
}

void RecordTransitionOutOfIdleMinute() {
  base::UmaHistogramCustomTimes(kDeviceActiveClientTransitionOutOfIdleMinute,
                                GetCurrentMinute(), base::Minutes(0),
                                base::Minutes(59),
                                60 /* number of histogram buckets */);
}

void RecordTransitionToCheckInMinute() {
  base::UmaHistogramCustomTimes(kDeviceActiveClientTransitionToCheckInMinute,
                                GetCurrentMinute(), base::Minutes(0),
                                base::Minutes(59),
                                60 /* number of histogram buckets */);
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

}  // namespace

// static
void DeviceActivityClient::RecordDeviceActivityMethodCalled(
    DeviceActivityMethod method_name) {
  // Record the device activity method calls.
  const char kDeviceActivityMethodCalled[] = "Ash.DeviceActivity.MethodCalled";

  base::UmaHistogramEnumeration(kDeviceActivityMethodCalled, method_name);
}

DeviceActivityClient::DeviceActivityClient(
    NetworkStateHandler* handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<PsmDelegate> psm_delegate,
    std::unique_ptr<base::RepeatingTimer> report_timer,
    const std::string& fresnel_base_url,
    const std::string& api_key,
    std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases)
    : network_state_handler_(handler),
      url_loader_factory_(url_loader_factory),
      psm_delegate_(std::move(psm_delegate)),
      report_timer_(std::move(report_timer)),
      fresnel_base_url_(fresnel_base_url),
      api_key_(api_key),
      use_cases_(std::move(use_cases)) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientConstructor);

  DCHECK(network_state_handler_);
  DCHECK(url_loader_factory_);
  DCHECK(psm_delegate_);
  DCHECK(report_timer_);
  DCHECK(!use_cases_.empty());

  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &DeviceActivityClient::ReportUseCases);

  network_state_handler_->AddObserver(this, FROM_HERE);
  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

DeviceActivityClient::~DeviceActivityClient() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientDestructor);

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
  else
    OnNetworkOffline();
}

DeviceActivityClient::State DeviceActivityClient::GetState() const {
  return state_;
}

std::vector<DeviceActiveUseCase*> DeviceActivityClient::GetUseCases() const {
  std::vector<DeviceActiveUseCase*> use_cases_ptr;

  for (auto& use_case : use_cases_) {
    use_cases_ptr.push_back(use_case.get());
  }
  return use_cases_ptr;
}

void DeviceActivityClient::OnNetworkOnline() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnNetworkOnline);

  ReportUseCases();
}

void DeviceActivityClient::OnNetworkOffline() {
  CancelUseCases();
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
      [[fallthrough]];
    case State::kUnknown:
      NOTREACHED();
      break;
  }

  return base_url.ReplaceComponents(replacements);
}

// TODO(https://crbug.com/1262189): Add callback to report actives only after
// synchronizing the system clock.
void DeviceActivityClient::ReportUseCases() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientReportUseCases);

  DCHECK(!use_cases_.empty());

  if (!network_connected_ || state_ != State::kIdle) {
    TransitionToIdle(nullptr);
    return;
  }

  // The network is connected and the client |state_| is kIdle.
  last_transition_out_of_idle_time_ = base::Time::Now();

  for (auto& use_case : use_cases_) {
    // Ownership of the use cases will be maintained by the |use_cases_| vector.
    pending_use_cases_.push(use_case.get());
  }

  // Pop from |pending_use_cases_| queue in |TransitionToIdle|, after the
  // use case has tried to be reported.
  TransitionOutOfIdle(pending_use_cases_.front());
}

void DeviceActivityClient::CancelUseCases() {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientCancelUseCases);

  // Use RAII to reset |url_loader_| after current function scope.
  // Delete |url_loader_| before the callback is invoked cancels the sent out
  // request.
  // No callback will be invoked in the case a network request is sent,
  // and the device internet disconnects.
  auto url_loader = std::move(url_loader_);

  // Use RAII to clear the queue.
  std::queue<DeviceActiveUseCase*> pending_use_cases;
  std::swap(pending_use_cases_, pending_use_cases);

  // Calling std::queue.front() on empty queue results in undefined behaviour.
  // Safety check queue is not empty before |TransitionToIdle|.
  if (pending_use_cases.empty()) {
    return;
  }

  TransitionToIdle(pending_use_cases.front());
}

void DeviceActivityClient::TransitionOutOfIdle(
    DeviceActiveUseCase* current_use_case) {
  RecordTransitionOutOfIdleMinute();
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionOutOfIdle);

  DCHECK(current_use_case);

  // Begin phase one of checking membership if the device has not pinged yet
  // within the given use case window.
  // TODO(https://crbug.com/1262187): Remove hardcoded use case when adding
  // support for additional use cases (i.e MONTHLY, ALL_TIME, etc.).
  if (current_use_case->IsDevicePingRequired(
          last_transition_out_of_idle_time_)) {
    current_use_case->SetWindowIdentifier(
        current_use_case->GenerateUTCWindowIdentifier(
            last_transition_out_of_idle_time_));
    auto current_psm_id = current_use_case->GetPsmIdentifier();

    // Check if the PSM id is generated.
    if (!current_psm_id.has_value()) {
      TransitionToIdle(current_use_case);
      return;
    }

    std::vector<psm_rlwe::RlwePlaintextId> psm_rlwe_ids = {
        current_psm_id.value()};
    auto status_or_client = psm_delegate_->CreatePsmClient(
        current_use_case->GetPsmUseCase(), psm_rlwe_ids);

    if (!status_or_client.ok()) {
      TransitionToIdle(current_use_case);
      return;
    }

    current_use_case->SetPsmRlweClient(std::move(status_or_client.value()));

    switch (current_use_case->GetPsmUseCase()) {
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY:
        if (base::FeatureList::IsEnabled(
                features::kDeviceActiveClientDailyCheckMembership)) {
          TransitionToCheckMembershipOprf(current_use_case);
          return;
        } else {
          // During rollout, we perform CheckIn without CheckMembership for
          // powerwash, recovery, or RMA devices.
          TransitionToCheckIn(current_use_case);
          return;
        }
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY:
        if (base::FeatureList::IsEnabled(
                features::kDeviceActiveClientMonthlyCheckIn)) {
          // During rollout, we perform CheckIn without CheckMembership for
          // powerwash, recovery, or RMA devices.
          TransitionToCheckIn(current_use_case);
          return;
        }

        if (base::FeatureList::IsEnabled(
                features::kDeviceActiveClientMonthlyCheckMembership)) {
          TransitionToCheckMembershipOprf(current_use_case);
          return;
        }

        break;
      default:
        VLOG(1) << "Use case is not supported yet. "
                << psm_rlwe::RlweUseCase_Name(
                       current_use_case->GetPsmUseCase());
        TransitionToIdle(current_use_case);
        return;
    }
  }

  TransitionToIdle(current_use_case);
}

void DeviceActivityClient::TransitionToHealthCheck() {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToHealthCheck);

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
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnHealthCheckDone);

  DCHECK_EQ(state_, State::kHealthCheck);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Record duration of |kHealthCheck| state.
  RecordDurationStateMetric(state_, state_timer_.Elapsed());

  // Transition back to kIdle state after performing a health check on servers.
  TransitionToIdle(nullptr);
}

void DeviceActivityClient::TransitionToCheckMembershipOprf(
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckMembershipOprf);

  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipOprf;

  // Report UMA histogram for transitioning state to |kCheckingMembershipOprf|.
  RecordStateCountMetric(state_);

  // Generate PSM Oprf request body.
  const auto status_or_oprf_request =
      current_use_case->GetPsmRlweClient()->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
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
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipOprfDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnCheckMembershipOprfDone);

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
    TransitionToIdle(current_use_case);
    return;
  }

  // Parse |fresnel_oprf_response| for oprf_response.
  if (!psm_oprf_response.has_rlwe_oprf_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
      psm_oprf_response.rlwe_oprf_response();

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToCheckMembershipQuery(oprf_response, current_use_case);
}

void DeviceActivityClient::TransitionToCheckMembershipQuery(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response,
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckMembershipQuery);

  DCHECK_EQ(state_, State::kCheckingMembershipOprf);
  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingMembershipQuery;

  // Report UMA histogram for transitioning state to |kCheckingMembershipQuery|.
  RecordStateCountMetric(state_);

  // Generate PSM Query request body.
  const auto status_or_query_request =
      current_use_case->GetPsmRlweClient()->CreateQueryRequest(oprf_response);
  if (!status_or_query_request.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
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
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckMembershipQueryDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnCheckMembershipQueryDone);

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
    TransitionToIdle(current_use_case);
    return;
  }

  // Parse |fresnel_query_response| for psm query_response.
  if (!psm_query_response.has_rlwe_query_response()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
      psm_query_response.rlwe_query_response();

  auto status_or_response =
      current_use_case->GetPsmRlweClient()->ProcessQueryResponse(
          query_response);
  if (!status_or_response.ok()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
    return;
  }

  // Ensure the existence of one membership response. Then, verify that it is
  // regarding the current PSM ID.
  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      status_or_response.value();
  if (rlwe_membership_responses.membership_responses_size() != 1 ||
      rlwe_membership_responses.membership_responses(0)
              .plaintext_id()
              .sensitive_id() !=
          current_use_case->GetPsmIdentifier().value().sensitive_id()) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
    return;
  }

  private_membership::MembershipResponse membership_response =
      rlwe_membership_responses.membership_responses(0).membership_response();

  bool is_psm_id_member = membership_response.is_member();

  // Record the query membership result to UMA histogram.
  RecordQueryMembershipResultBoolean(is_psm_id_member);

  if (!is_psm_id_member) {
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToCheckIn(current_use_case);
    return;
  } else {
    // Update local state to signal ping has already been sent for use case
    // window.
    current_use_case->SetLastKnownPingTimestamp(
        last_transition_out_of_idle_time_);
    RecordDurationStateMetric(state_, state_timer_.Elapsed());
    TransitionToIdle(current_use_case);
    return;
  }
}

void DeviceActivityClient::TransitionToCheckIn(
    DeviceActiveUseCase* current_use_case) {
  RecordTransitionToCheckInMinute();
  RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientTransitionToCheckIn);

  DCHECK(!url_loader_);

  state_timer_ = base::ElapsedTimer();

  // |state_| must be set correctly in order to generate correct URL.
  state_ = State::kCheckingIn;

  // Report UMA histogram for transitioning state to |kCheckingIn|.
  RecordStateCountMetric(state_);

  // Generate Fresnel PSM import request body.
  device_activity::ImportDataRequest import_request =
      current_use_case->GenerateImportRequestBody();

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
                     weak_factory_.GetWeakPtr(), current_use_case),
      kMaxFresnelResponseSizeBytes);
}

void DeviceActivityClient::OnCheckInDone(
    DeviceActiveUseCase* current_use_case,
    std::unique_ptr<std::string> response_body) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientOnCheckInDone);

  DCHECK_EQ(state_, State::kCheckingIn);

  // Use RAII to reset |url_loader_| after current function scope.
  // Resetting |url_loader_| also invalidates the |response_info| variable.
  auto url_loader = std::move(url_loader_);

  int net_code = url_loader->NetError();
  RecordResponseStateMetric(state_, net_code);

  // Successful import request - PSM ID was imported successfully.
  if (net_code == net::OK) {
    // Update local state pref to record reporting device active.
    current_use_case->SetLastKnownPingTimestamp(
        last_transition_out_of_idle_time_);
  }

  RecordDurationStateMetric(state_, state_timer_.Elapsed());
  TransitionToIdle(current_use_case);
}

void DeviceActivityClient::TransitionToIdle(
    DeviceActiveUseCase* current_use_case) {
  RecordDeviceActivityMethodCalled(DeviceActivityClient::DeviceActivityMethod::
                                       kDeviceActivityClientTransitionToIdle);

  DCHECK(!url_loader_);
  state_ = State::kIdle;

  if (current_use_case) {
    // This will also reset the |current_use_case| psm_id field.
    current_use_case->SetWindowIdentifier(absl::nullopt);
    current_use_case = nullptr;

    // Pop the front of the queue since the use case has tried reporting.
    if (!pending_use_cases_.empty())
      pending_use_cases_.pop();
  }

  // Try to report the remaining pending use cases.
  if (!pending_use_cases_.empty()) {
    TransitionOutOfIdle(pending_use_cases_.front());
    return;
  }

  // Report UMA histogram for transitioning state back to |kIdle|.
  RecordStateCountMetric(state_);
}

}  // namespace device_activity
}  // namespace ash

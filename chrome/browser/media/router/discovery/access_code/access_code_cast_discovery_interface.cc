// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"

#include <cstddef>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "AccessCodeCastDiscoveryInterface";

using AddSinkResultCode = access_code_cast::mojom::AddSinkResultCode;

constexpr base::TimeDelta kTimeout = base::Milliseconds(30000);

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_cast_discovery_api",
                                        R"(
      semantics {
        sender: "Chrome Cast2Class Discovery Interface"
        description:
          "A user will be able to cast to cast devices that do not appear in"
          "the Google Cast menu, using either the access code or QR code"
          "displayed on the cast devices's screen. The access code or QR code"
          "will be sent to a discovery server that will confirm that the"
          "inputted pin of a user corresponds to a registered chromecast device"
          "stored on the discovery server."
        trigger:
          "The request is triggered when a user attempts to start a casting"
          "session with an access code or QR code from the Google cast menu."
        data:
          "The transmitted information is a sanitized pin."
        destination: GOOGLE_OWNED_SERVICE
      }
        policy {
          cookies_allowed: NO
          setting:
            "No setting. The feature does nothing by default. Users must take"
            "an explicit action to trigger it."
          chrome_policy {
            AccessCodeCastEnabled {
                AccessCodeCastEnabled: false
            }
          }
        }
  )");

std::string GetDiscoveryUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (IsCommandLineSwitchSupported() &&
      command_line->HasSwitch(switches::kDiscoveryEndpointSwitch)) {
    return command_line->GetSwitchValueASCII(
        switches::kDiscoveryEndpointSwitch);
  }

  return std::string(kDefaultDiscoveryEndpoint) + kDiscoveryServicePath;
}

}  // namespace

AccessCodeCastDiscoveryInterface::AccessCodeCastDiscoveryInterface(
    Profile* profile,
    const std::string& access_code,
    LoggerImpl* logger,
    signin::IdentityManager* identity_manager)
    : profile_(profile),
      access_code_(access_code),
      logger_(logger),
      identity_manager_(identity_manager),
      endpoint_fetcher_(CreateEndpointFetcher(access_code)) {
  DCHECK(profile_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

AccessCodeCastDiscoveryInterface::~AccessCodeCastDiscoveryInterface() = default;

void AccessCodeCastDiscoveryInterface::ReportErrorViaCallback(
    AddSinkResultCode error) {
  if (callback_.is_null()) {
    return;
  }
  std::move(callback_).Run(std::nullopt, error);
}

AddSinkResultCode AccessCodeCastDiscoveryInterface::GetErrorFromResponse(
    const base::Value& response) {
  const base::Value::Dict* error = response.GetDict().FindDict(kJsonError);
  if (!error) {
    return AddSinkResultCode::OK;
  }

  // Get the HTTP code
  std::optional<int> http_code = error->FindInt(kJsonErrorCode);
  if (!http_code) {
    return AddSinkResultCode::RESPONSE_MALFORMED;
  }

  const std::string* error_message = error->FindString(kJsonErrorMessage);

  logger_->LogError(
      mojom::LogCategory::kDiscovery, kLoggerComponent,
      "The server response yielded the error: " + std::string(*error_message) +
          " with HTTP code: " + base::NumberToString(*http_code),
      "", "", "");

  switch (*http_code) {
    // 401
    case net::HTTP_UNAUTHORIZED:
      ABSL_FALLTHROUGH_INTENDED;
    // 403
    case net::HTTP_FORBIDDEN:
      return AddSinkResultCode::AUTH_ERROR;

    // 404
    case net::HTTP_NOT_FOUND:
      return AddSinkResultCode::ACCESS_CODE_NOT_FOUND;

    // 408
    case net::HTTP_REQUEST_TIMEOUT:
      ABSL_FALLTHROUGH_INTENDED;
    // 502
    case net::HTTP_GATEWAY_TIMEOUT:
      return AddSinkResultCode::SERVER_ERROR;

    // 412
    case net::HTTP_PRECONDITION_FAILED:
      ABSL_FALLTHROUGH_INTENDED;
    // 417
    case net::HTTP_EXPECTATION_FAILED:
      return AddSinkResultCode::INVALID_ACCESS_CODE;

    // 429
    case net::HTTP_TOO_MANY_REQUESTS:
      return AddSinkResultCode::TOO_MANY_REQUESTS;

    // 501
    case net::HTTP_INTERNAL_SERVER_ERROR:
      return AddSinkResultCode::SERVER_ERROR;

    // 503
    case net::HTTP_SERVICE_UNAVAILABLE:
      return AddSinkResultCode::SERVICE_NOT_PRESENT;

    case net::HTTP_OK:
      NOTREACHED_IN_MIGRATION();
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return AddSinkResultCode::HTTP_RESPONSE_CODE_ERROR;
  }
}

// TODO(b/206997996): Add an enum to the EndpointResponse struct so that we can
// check the enum instead of the string
AddSinkResultCode AccessCodeCastDiscoveryInterface::IsResponseValid(
    const std::optional<base::Value>& response) {
  if (!response || !response->is_dict()) {
    logger_->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The response body from the server was of unexpected format.", "", "",
        "");
    return AddSinkResultCode::RESPONSE_MALFORMED;
  }

  if (response->GetDict().empty()) {
    logger_->LogError(mojom::LogCategory::kDiscovery, kLoggerComponent,
                      "The response from the server does not have a value. "
                      "Server response is: " +
                          response->DebugString(),
                      "", "", "");
    return AddSinkResultCode::EMPTY_RESPONSE;
  }

  return GetErrorFromResponse(*response);
}

void AccessCodeCastDiscoveryInterface::SetDeviceCapabilitiesField(
    chrome_browser_media::proto::DeviceCapabilities* device_proto,
    bool value,
    const std::string& key) {
  if (key == kJsonVideoOut) {
    device_proto->set_video_out(value);
  } else if (key == kJsonVideoIn) {
    device_proto->set_video_in(value);
  } else if (key == kJsonAudioOut) {
    device_proto->set_audio_out(value);
  } else if (key == kJsonAudioIn) {
    device_proto->set_audio_in(value);
  } else if (key == kJsonDevMode) {
    device_proto->set_dev_mode(value);
  }
}

void AccessCodeCastDiscoveryInterface::SetNetworkInfoField(
    chrome_browser_media::proto::NetworkInfo* network_proto,
    const std::string& value,
    const std::string& key) {
  if (key == kJsonHostName) {
    network_proto->set_host_name(value);
  } else if (key == kJsonPort) {
    network_proto->set_port(value);
  } else if (key == kJsonIpV4Address) {
    network_proto->set_ip_v4_address(value);
  } else if (key == kJsonIpV6Address) {
    network_proto->set_ip_v6_address(value);
  }
}

std::unique_ptr<EndpointFetcher>
AccessCodeCastDiscoveryInterface::CreateEndpointFetcher(
    const std::string& access_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<std::string> discovery_scopes;
  discovery_scopes.push_back(kDiscoveryOAuth2Scope);

  // TODO(crbug.com/40067771): ConsentLevel::kSync is deprecated and should be
  //     removed. See ConsentLevel::kSync documentation for details.
  return std::make_unique<EndpointFetcher>(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      kDiscoveryOAuthConsumerName,
      GURL(base::StrCat({GetDiscoveryUrl(), "/", access_code})), kGetMethod,
      kContentType, discovery_scopes, kTimeout, kEmptyPostData,
      kTrafficAnnotation, identity_manager_, signin::ConsentLevel::kSync);
}

void AccessCodeCastDiscoveryInterface::ValidateDiscoveryAccessCode(
    DiscoveryDeviceCallback callback) {
  DCHECK(!callback_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_ = std::move(callback);

  auto* const fetcher_ptr = endpoint_fetcher_.get();
  fetcher_ptr->Fetch(
      base::BindOnce(&AccessCodeCastDiscoveryInterface::HandleServerResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<EndpointFetcher>
AccessCodeCastDiscoveryInterface::CreateEndpointFetcherForTesting(
    const std::string& access_code) {
  return CreateEndpointFetcher(access_code);
}

void AccessCodeCastDiscoveryInterface::HandleServerErrorForTesting(
    std::unique_ptr<EndpointResponse> endpoint_response) {
  HandleServerError(std::move(endpoint_response));
}

void AccessCodeCastDiscoveryInterface::HandleServerResponse(
    std::unique_ptr<EndpointResponse> response) {
  if (response->error_type.has_value()) {
    HandleServerError(std::move(response));
    return;
  }

  std::optional<base::Value> response_value =
      base::JSONReader::Read(response->response);

  AddSinkResultCode result_code = IsResponseValid(response_value);
  if (result_code != AddSinkResultCode::OK) {
    logger_->LogError(mojom::LogCategory::kDiscovery, kLoggerComponent,
                      "The response string from the server was not valid", "",
                      "", "");
    ReportErrorViaCallback(result_code);
    return;
  }

  std::pair<std::optional<DiscoveryDevice>, AddSinkResultCode>
      construction_result =
          ConstructDiscoveryDeviceFromJson(std::move(response_value.value()));
  std::move(callback_).Run(construction_result.first,
                           construction_result.second);
}

void AccessCodeCastDiscoveryInterface::HandleServerError(
    std::unique_ptr<EndpointResponse> response) {
  if (!response->error_type.has_value()) {
    return;
  }

  auto error_type = response->error_type.value();

  switch (error_type) {
    case FetchErrorType::kAuthError:
      if (response->response == "No primary accounts found") {
        logger_->LogError(mojom::LogCategory::kDiscovery, kLoggerComponent,
                          "The account needs to have sync enabled.", "", "",
                          "");
        ReportErrorViaCallback(AddSinkResultCode::PROFILE_SYNC_ERROR);
      } else {
        logger_->LogError(
            mojom::LogCategory::kDiscovery, kLoggerComponent,
            "The request to the server failed to be authenticated.", "", "",
            "");
        ReportErrorViaCallback(AddSinkResultCode::AUTH_ERROR);
      }
      break;

    case FetchErrorType::kNetError:
      logger_->LogError(mojom::LogCategory::kDiscovery, kLoggerComponent,
                        "Did not receive a response from server while "
                        "attempting to validate discovery device.",
                        "", "", "");
      ReportErrorViaCallback(AddSinkResultCode::SERVER_ERROR);
      break;

    case FetchErrorType::kResultParseError:
      logger_->LogError(
          mojom::LogCategory::kDiscovery, kLoggerComponent,
          "The server response was incorrectly formatted/malformed "
          "and we are not able to use it.",
          "", "", "");
      ReportErrorViaCallback(AddSinkResultCode::RESPONSE_MALFORMED);
      break;

    default:
      logger_->LogError(
          mojom::LogCategory::kDiscovery, kLoggerComponent,
          base::StringPrintf("An unknown error occurred. HTTP Status "
                             "of the response is: %d",
                             response->http_status_code),
          "", "", "");
      ReportErrorViaCallback(AddSinkResultCode::SERVER_ERROR);
  }
}

std::pair<std::optional<AccessCodeCastDiscoveryInterface::DiscoveryDevice>,
          AccessCodeCastDiscoveryInterface::AddSinkResultCode>
AccessCodeCastDiscoveryInterface::ConstructDiscoveryDeviceFromJson(
    base::Value json_response) {
  DiscoveryDevice discovery_device;

  base::Value::Dict* device = json_response.GetDict().FindDict(kJsonDevice);
  if (!device) {
    return std::make_pair(std::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  std::string* display_name = device->FindString(kJsonDisplayName);
  if (!display_name) {
    return std::make_pair(std::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  std::string* sink_id = device->FindString(kJsonId);
  if (!sink_id) {
    return std::make_pair(std::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  chrome_browser_media::proto::DeviceCapabilities device_capabilities_proto;
  base::Value::Dict* device_capabilities =
      device->FindDict(kJsonDeviceCapabilities);
  if (!device_capabilities) {
    return std::make_pair(std::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }
  const auto capability_keys = {kJsonVideoOut, kJsonVideoIn, kJsonAudioOut,
                                kJsonAudioIn, kJsonDevMode};

  for (auto* const capability_key : capability_keys) {
    std::optional<bool> capability =
        device_capabilities->FindBool(capability_key);
    if (capability.has_value()) {
      SetDeviceCapabilitiesField(&device_capabilities_proto, capability.value(),
                                 capability_key);
    } else if (device_capabilities->contains(capability_key)) {
      // It's ok if the capability isn't present, but if it is, it must be a
      // bool
      return std::make_pair(std::nullopt,
                            AddSinkResultCode::RESPONSE_MALFORMED);
    }
  }

  chrome_browser_media::proto::NetworkInfo network_info_proto;
  base::Value::Dict* network_info = device->FindDict(kJsonNetworkInfo);
  if (!network_info) {
    return std::make_pair(std::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }
  const auto network_keys = {kJsonHostName, kJsonPort, kJsonIpV4Address,
                             kJsonIpV6Address};
  for (auto* const network_key : network_keys) {
    std::string* network_value = network_info->FindString(network_key);
    if (network_value) {
      SetNetworkInfoField(&network_info_proto, *network_value, network_key);
    }
  }

  discovery_device.set_display_name(*display_name);
  discovery_device.set_id(*sink_id);
  *discovery_device.mutable_device_capabilities() = device_capabilities_proto;
  *discovery_device.mutable_network_info() = network_info_proto;

  return std::make_pair(std::move(discovery_device), AddSinkResultCode::OK);
}

}  // namespace media_router

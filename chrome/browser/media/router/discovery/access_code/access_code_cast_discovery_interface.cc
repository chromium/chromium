// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"

#include <cstddef>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_router {

namespace {

// TODO(b/206131520): Add Policy Switches to
// AccessCodeCastDiscoveryInterface.
constexpr char kGetMethod[] = "GET";
constexpr char kContentType[] = "application/json; charset=UTF-8";
constexpr char kDiscoveryOAuth2Scope[] =
    "https://www.googleapis.com/auth/applicense.bytebot";
constexpr char kDiscoveryEndpoint[] =
    "https://castedumessaging-pa.googleapis.com/";
constexpr char kDiscoveryServicePath[] = "v1/receivers/";
constexpr char kDiscoveryServiceQuery[] = "?checkOnly=true&access_token=";
constexpr char kDiscoveryOAuthConsumerName[] = "access_code_cast_discovery";
constexpr char kEmptyPostData[] = "";

constexpr char kJsonDevice[] = "device";
constexpr char kJsonDisplayName[] = "displayName";
constexpr char kJsonId[] = "id";

constexpr char kJsonNetworkInfo[] = "networkInfo";
constexpr char kJsonHostName[] = "hostName";
constexpr char kJsonPort[] = "port";
constexpr char kJsonIpV4Address[] = "ipV4Address";
constexpr char kJsonIpV6Address[] = "ipV6Address";

constexpr char kJsonDeviceCapabilities[] = "deviceCapabilities";
constexpr char kJsonVideoOut[] = "videoOut";
constexpr char kJsonVideoIn[] = "videoIn";
constexpr char kJsonAudioOut[] = "audioOut";
constexpr char kJsonAudioIn[] = "audioIn";
constexpr char kJsonDevMode[] = "devMode";

const int64_t kTimeoutMs = 30000;

const GURL GetDiscoveryEndpoint() {
  return GURL(base::StrCat({kDiscoveryEndpoint, kDiscoveryServicePath}));
}

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

// TODO(b/206997996): Add an enum to the EndpointResponse struct so that we can
// check the enum instead of the string
bool IsResponseValid(const absl::optional<base::Value>& response) {
  if (!response || !response->is_dict()) {
    DVLOG(1) << "response_body was of unexpected format.";
    return false;
  }

  if (response->DictEmpty()) {
    DVLOG(1) << "Response does not have value. Response: "
             << response->DebugString();
    return false;
  }
  return true;
}

bool HasAuthenticationError(const std::string& response) {
  return response == "There was an authentication error";
}

bool HasServerError(const std::string& response) {
  return response == "There was a response error";
}

}  // namespace

AccessCodeCastDiscoveryInterface::AccessCodeCastDiscoveryInterface(
    Profile* profile,
    const std::string& access_code)
    : profile_(profile),
      access_code_(access_code),
      endpoint_fetcher_(CreateEndpointFetcher(access_code)),
      discovery_url_(GetDiscoveryEndpoint()) {
  DCHECK(profile_);
}

AccessCodeCastDiscoveryInterface::AccessCodeCastDiscoveryInterface(
    Profile* profile,
    const std::string& access_code,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher)
    : profile_(profile),
      access_code_(access_code),
      endpoint_fetcher_(std::move(endpoint_fetcher)),
      discovery_url_(GetDiscoveryEndpoint()) {
  DCHECK(profile_);
}

AccessCodeCastDiscoveryInterface::~AccessCodeCastDiscoveryInterface() = default;

void AccessCodeCastDiscoveryInterface::ReportError(AddSinkResultCode error) {
  std::move(callback_).Run(absl::nullopt, error);
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
  std::vector<std::string> discovery_scopes;
  discovery_scopes.push_back(kDiscoveryOAuth2Scope);

  return std::make_unique<EndpointFetcher>(
      profile_, kDiscoveryOAuthConsumerName,
      GURL(base::StrCat(
          {discovery_url_.spec(), access_code, kDiscoveryServiceQuery})),
      kGetMethod, kContentType, discovery_scopes, kTimeoutMs, kEmptyPostData,
      kTrafficAnnotation);
}

void AccessCodeCastDiscoveryInterface::ValidateDiscoveryAccessCode(
    DiscoveryDeviceCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  auto* const fetcher_ptr = endpoint_fetcher_.get();
  fetcher_ptr->Fetch(
      base::BindOnce(&AccessCodeCastDiscoveryInterface::HandleServerResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastDiscoveryInterface::HandleServerResponse(
    std::unique_ptr<EndpointResponse> response) {
  const std::string& response_string = response->response;
  if (HasAuthenticationError(response_string)) {
    DVLOG(1) << "The request to the server failed to be authenticated";
    ReportError(AddSinkResultCode::AUTH_ERROR);
    return;
  }

  if (HasServerError(response_string)) {
    DVLOG(1) << "Did not recieve a response from server while attempting to"
             << " validate discovery device.";
    ReportError(AddSinkResultCode::SERVER_ERROR);
    return;
  }

  absl::optional<base::Value> response_value =
      base::JSONReader::Read(response->response);
  if (!IsResponseValid(response_value)) {
    ReportError(AddSinkResultCode::RESPONSE_MALFORMED);
    return;
  }

  std::pair<absl::optional<DiscoveryDevice>, AddSinkResultCode>
      construction_result =
          ConstructDiscoveryDeviceFromJson(std::move(response_value.value()));
  std::move(callback_).Run(construction_result.first,
                           construction_result.second);
}

std::pair<absl::optional<AccessCodeCastDiscoveryInterface::DiscoveryDevice>,
          AccessCodeCastDiscoveryInterface::AddSinkResultCode>
AccessCodeCastDiscoveryInterface::ConstructDiscoveryDeviceFromJson(
    base::Value json_response) {
  DiscoveryDevice discovery_device;

  base::Value* device = json_response.FindKey(kJsonDevice);
  if (!device) {
    return std::make_pair(absl::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  std::string* display_name = device->FindStringKey(kJsonDisplayName);
  if (!display_name) {
    return std::make_pair(absl::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  std::string* sink_id = device->FindStringKey(kJsonId);
  if (!sink_id) {
    return std::make_pair(absl::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }

  chrome_browser_media::proto::DeviceCapabilities device_capabilities_proto;
  base::Value* device_capabilities = device->FindKey(kJsonDeviceCapabilities);
  if (!device_capabilities) {
    return std::make_pair(absl::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }
  const auto capability_keys = {kJsonVideoOut, kJsonVideoIn, kJsonAudioOut,
                                kJsonAudioIn, kJsonDevMode};

  for (auto* const capability_key : capability_keys) {
    absl::optional<bool> capability =
        device_capabilities->FindBoolKey(capability_key);
    if (capability.has_value()) {
      SetDeviceCapabilitiesField(&device_capabilities_proto, capability.value(),
                                 capability_key);
    } else {
      return std::make_pair(absl::nullopt,
                            AddSinkResultCode::RESPONSE_MALFORMED);
    }
  }

  chrome_browser_media::proto::NetworkInfo network_info_proto;
  base::Value* network_info = device->FindKey(kJsonNetworkInfo);
  if (!network_info) {
    return std::make_pair(absl::nullopt, AddSinkResultCode::RESPONSE_MALFORMED);
  }
  const auto network_keys = {kJsonHostName, kJsonPort, kJsonIpV4Address,
                             kJsonIpV6Address};
  for (auto* const network_key : network_keys) {
    std::string* network_value = network_info->FindStringKey(network_key);
    if (network_value) {
      SetNetworkInfoField(&network_info_proto, *network_value, network_key);
    } else {
      return std::make_pair(absl::nullopt,
                            AddSinkResultCode::RESPONSE_MALFORMED);
    }
  }

  discovery_device.set_display_name(*display_name);
  discovery_device.set_id(*sink_id);
  *discovery_device.mutable_device_capabilities() = device_capabilities_proto;
  *discovery_device.mutable_network_info() = network_info_proto;

  return std::make_pair(std::move(discovery_device), AddSinkResultCode::OK);
}

}  // namespace media_router

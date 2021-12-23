// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"

#include <memory>
#include <utility>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"

namespace {
// Maximum accepted size of an API response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;
const char server_url[] =
    "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("photos_service", R"(
      semantics {
        sender: "Photos Service"
        description:
          "The Photos Service requests Memories from "
          "the Google Photos API. The response will be displayed in NTP's "
          "Photos Module."
        trigger:
          "Each time a user navigates to the NTP while "
          "the Photos module is enabled and the user is "
          "signed in."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can control this feature by (1) selecting "
          "a non-Google default search engine in Chrome "
          "settings under 'Search Engine', (2) signing out, "
          "or (3) disabling the Drive module."
        chrome_policy {
          DefaultSearchProviderEnabled {
            policy_options {mode: MANDATORY}
            DefaultSearchProviderEnabled: false
          }
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
          NTPCardsVisible {
            NTPCardsVisible: false
          }
        }
      })");
constexpr char kMemoryTemplate[] = R"(
  {
    "memoryMediaKey": "key%d",
    "title": {
      "header": "%d years ago",
      "subheader": ""
    },
    "coverMediaKey": "coverKey%d",
    "coverDatUrl": "https://lh3.googleusercontent.com/proxy/CyeQrfWvSkJ-4wjGmm1zVIP4XZKL4oAjywWcPh8lhrwtizOY4kGsDtVa3nk984qJB5q2-r7aInfG25UFjfwyu7QEraqepTlbsDdKX1yeenhh7EGeAR2Hp1QcbO24C7WyU8bLPx8o_2HA-opm6cqZ8f4ehEXCxMEbR79A44jcWpacTLfYERPGeVrljo2vAl2LyFMHrA"
  })";
}  // namespace

// static
const char PhotosService::kLastDismissedTimePrefName[] =
    "NewTabPage.Photos.LastDimissedTime";
const char PhotosService::kOptInAcknowledgedPrefName[] =
    "NewTabPage.Photos.OptInAcknowledged";
const char PhotosService::kLastMemoryOpenTimePrefName[] =
    "NewTabPage.Photos.LastMemoryOpenTime";

// static
const base::TimeDelta PhotosService::kDismissDuration = base::Days(1);

PhotosService::PhotosService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      pref_service_(pref_service) {
  identity_manager_->AddObserver(this);
}

PhotosService::~PhotosService() {
  identity_manager_->RemoveObserver(this);
}

// static
void PhotosService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastDismissedTimePrefName, base::Time());
  registry->RegisterBooleanPref(kOptInAcknowledgedPrefName, false);
  registry->RegisterTimePref(kLastMemoryOpenTimePrefName, base::Time());
}

void PhotosService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // If the primary account has changed, clear opt-in pref.
  if (event.GetCurrentState().primary_account !=
          event.GetPreviousState().primary_account &&
      pref_service_->HasPrefPath(kOptInAcknowledgedPrefName)) {
    pref_service_->ClearPref(kOptInAcknowledgedPrefName);
  }
}

void PhotosService::GetMemories(GetMemoriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.push_back(std::move(callback));
  if (callbacks_.size() > 1) {
    return;
  }

  // Bail if module is still dismissed.
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned) &&
      !pref_service_->GetTime(kLastDismissedTimePrefName).is_null() &&
      base::Time::Now() - pref_service_->GetTime(kLastDismissedTimePrefName) <
          kDismissDuration) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    }
    callbacks_.clear();
    return;
  }

  // Skip fetch and jump straight to data parsing when serving fake data.
  std::string fake_data_choice = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpPhotosModule, ntp_features::kNtpPhotosModuleDataParam);
  if (fake_data_choice != "") {
    std::string fake_response = "{\"memory\": [";
    int num_memory;
    base::StringToInt(fake_data_choice, &num_memory);
    for (int i = 0; i < num_memory; i++) {
      std::string memory = base::StringPrintf(kMemoryTemplate, i, i + 2, i);
      if (i + 1 < num_memory) {
        memory += ", ";
      }
      fake_response += memory;
    }
    fake_response += "]}";

    data_decoder::DataDecoder::ParseJsonIsolated(
        fake_response, base::BindOnce(&PhotosService::OnJsonParsed,
                                      weak_factory_.GetWeakPtr(), ""));
    return;
  }

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kPhotosModuleOAuth2Scope);
  scopes.insert(GaiaConstants::kPhotosModuleImageOAuth2Scope);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_photos_module", identity_manager_, scopes,
      base::BindOnce(&PhotosService::OnTokenReceived,
                     weak_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
}

void PhotosService::DismissModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time::Now());
}

void PhotosService::RestoreModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time());
}

bool PhotosService::ShouldShowOptInScreen() {
  return !pref_service_->GetBoolean(kOptInAcknowledgedPrefName);
}

void PhotosService::OnUserOptIn(bool accept) {
  pref_service_->SetBoolean(kOptInAcknowledgedPrefName, accept);
}

void PhotosService::OnMemoryOpen() {
  pref_service_->SetTime(kLastMemoryOpenTimePrefName, base::Time::Now());
}

void PhotosService::OnTokenReceived(GoogleServiceAuthError error,
                                    signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    }
    callbacks_.clear();
    return;
  }

  std::string cache_bust_param = "";
  if (!pref_service_->GetTime(kLastMemoryOpenTimePrefName).is_null()) {
    cache_bust_param =
        "?lastViewed=" +
        base::NumberToString(
            pref_service_->GetTime(kLastMemoryOpenTimePrefName).ToTimeT());
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(server_url + cache_bust_param);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);
  variations::AppendVariationsHeaderUnknownSignedIn(
      resource_request->url,
      /* Modules are only shown in non-incognito. */
      variations::InIncognito::kNo, resource_request.get());

  if (url_loader_) {
    return;
  }
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PhotosService::OnJsonReceived, weak_factory_.GetWeakPtr(),
                     token_info.token),
      kMaxResponseSize);
}

void PhotosService::OnJsonReceived(
    const std::string& token,
    const std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  bool loaded_from_cache = url_loader_->LoadedFromCache();
  url_loader_.reset();

  if (!loaded_from_cache) {
    base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                             base::PersistentHash("photos"));
  }

  if (net_error != net::OK || !response_body) {
    base::UmaHistogramEnumeration("NewTabPage.Photos.DataRequest",
                                  RequestResult::kError);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    }
    callbacks_.clear();
    return;
  }

  if (loaded_from_cache) {
    base::UmaHistogramEnumeration("NewTabPage.Photos.DataRequest",
                                  RequestResult::kCached);
  } else {
    base::UmaHistogramEnumeration("NewTabPage.Photos.DataRequest",
                                  RequestResult::kSuccess);
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body, base::BindOnce(&PhotosService::OnJsonParsed,
                                     weak_factory_.GetWeakPtr(), token));
}

void PhotosService::OnJsonParsed(
    const std::string& token,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    }
    callbacks_.clear();
    return;
  }

  auto* memories = result.value->FindListPath("memory");
  if (!memories) {
    base::UmaHistogramCustomCounts("NewTabPage.Photos.DataResponseCount", 0, 0,
                                   10, 11);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    }
    callbacks_.clear();
    return;
  }
  std::vector<photos::mojom::MemoryPtr> memory_list;

  base::UmaHistogramCustomCounts("NewTabPage.Photos.DataResponseCount",
                                 memories->GetList().size(), 0, 10, 11);
  for (const auto& memory : memories->GetList()) {
    auto* title = memory.FindStringPath("title.header");
    auto* memory_id = memory.FindStringPath("memoryMediaKey");
    auto* cover_id = memory.FindStringPath("coverMediaKey");
    auto* cover_url = memory.FindStringPath("coverUrl");
    auto* cover_dat_url = memory.FindStringPath("coverDatUrl");
    if (!title || !memory_id || !cover_id || (!cover_url && !cover_dat_url)) {
      continue;
    }
    auto mojo_memory = photos::mojom::Memory::New();
    mojo_memory->id = *memory_id;
    mojo_memory->title = *title;
    mojo_memory->item_url =
        GURL("https://photos.google.com/memory/featured/" + *memory_id +
             "/photo/" + *cover_id + "?referrer=CHROME_NTP");
    if (cover_url) {
      mojo_memory->cover_url = GURL(*cover_url + "?access_token=" + token);
    } else if (cover_dat_url) {
      mojo_memory->cover_url = GURL(*cover_dat_url);
    }

    memory_list.push_back(std::move(mojo_memory));
  }
  for (auto& callback : callbacks_) {
    std::move(callback).Run(mojo::Clone(memory_list));
  }
  callbacks_.clear();
}

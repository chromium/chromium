// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_model_loader.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace safe_browsing {

// Model Loader strings
const size_t ModelLoader::kMaxModelSizeBytes = 150 * 1024;
const int ModelLoader::kClientModelFetchIntervalMs = 3600 * 1000;
const char ModelLoader::kClientModelUrlPrefix[] =
    "https://ssl.gstatic.com/safebrowsing/csd/";
const char ModelLoader::kClientModelNamePattern[] =
    "client_model_v5%s_variation_%d.pb";
const char ModelLoader::kClientModelFinchExperiment[] =
    "ClientSideDetectionModel";
const char ModelLoader::kClientModelFinchParam[] =
    "ModelNum";
const char kUmaModelDownloadResponseMetricName[] =
    "SBClientPhishing.ClientModelDownloadResponseOrErrorCode";


// static
int ModelLoader::GetModelNumber() {
  std::string num_str = variations::GetVariationParamValue(
      kClientModelFinchExperiment, kClientModelFinchParam);
  int model_number = 0;
  if (!base::StringToInt(num_str, &model_number)) {
    model_number = 0;  // Default model
  }
  return model_number;
}

// static
std::string ModelLoader::FillInModelName(bool is_extended_reporting,
                                         int model_number) {
  return base::StringPrintf(kClientModelNamePattern,
                            is_extended_reporting ? "_ext" : "", model_number);
}

// static
bool ModelLoader::ModelHasValidHashIds(const ClientSideModel& model) {
  const int max_index = model.hashes_size() - 1;
  for (int i = 0; i < model.rule_size(); ++i) {
    for (int j = 0; j < model.rule(i).feature_size(); ++j) {
      if (model.rule(i).feature(j) < 0 ||
          model.rule(i).feature(j) > max_index) {
        return false;
      }
    }
  }
  for (int i = 0; i < model.page_term_size(); ++i) {
    if (model.page_term(i) < 0 || model.page_term(i) > max_index) {
      return false;
    }
  }
  return true;
}

// Model name and URL are a function of is_extended_reporting and Finch.
ModelLoader::ModelLoader(
    base::Closure update_renderers_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool is_extended_reporting)
    : name_(FillInModelName(is_extended_reporting, GetModelNumber())),
      url_(kClientModelUrlPrefix + name_),
      update_renderers_callback_(update_renderers_callback),
      url_loader_factory_(url_loader_factory) {
  DCHECK(url_.is_valid());
}

// For testing only
ModelLoader::ModelLoader(
    base::Closure update_renderers_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& model_name)
    : name_(model_name),
      url_(kClientModelUrlPrefix + name_),
      update_renderers_callback_(update_renderers_callback),
      url_loader_factory_(url_loader_factory) {
  DCHECK(url_.is_valid());
}

ModelLoader::~ModelLoader() {
  // This must happen on the same sequence as ScheduleFetch because it
  // invalidates any WeakPtrs allocated there.
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());
}

void ModelLoader::StartFetch() {
  // Start fetching the model either from the cache or possibly from the
  // network if the model isn't in the cache.

  // TODO(nparker): If no profile needs this model, we shouldn't fetch it.
  // Then only re-fetch when a profile setting changes to need it.
  // This will save on the order of ~50KB/week/client of bandwidth.
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_module_loader", R"(
        semantics {
          sender: "Safe Browsing Service"
          description:
            "Safe Browsing downloads the latest client-side phishing detection "
            "model at startup. It uses this data on future page loads to "
            "determine if it looks like a phishing page."
          trigger:
            "At startup. Most of the time the data will be in cache, so the "
            "response will be small."
          data:
            "No user-controlled data or PII is sent. Only a static URL of the "
            "model which is provided by field study is passed."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by toggling 'Protect "
            "you and your device from dangerous sites' in Chromium settings "
            "under Privacy. This feature is enabled by default."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ModelLoader::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void ModelLoader::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());

  std::string data;
  if (response_body)
    data = std::move(*response_body.get());
  const bool is_success = url_loader_->NetError() == net::OK;
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      kUmaModelDownloadResponseMetricName, url_loader_->NetError(),
      response_code);

  // max_age is valid iff !0.
  base::TimeDelta max_age;
  if (is_success && net::HTTP_OK == response_code)
    url_loader_->ResponseInfo()->headers->GetMaxAgeValue(&max_age);

  std::unique_ptr<ClientSideModel> model(new ClientSideModel());
  ClientModelStatus model_status;
  if (!is_success || net::HTTP_OK != response_code) {
    model_status = MODEL_FETCH_FAILED;
  } else if (data.empty()) {
    model_status = MODEL_EMPTY;
  } else if (data.size() > kMaxModelSizeBytes) {
    model_status = MODEL_TOO_LARGE;
  } else if (!model->ParseFromString(data)) {
    model_status = MODEL_PARSE_ERROR;
  } else if (!model->IsInitialized() || !model->has_version()) {
    model_status = MODEL_MISSING_FIELDS;
  } else if (!ModelHasValidHashIds(*model)) {
    model_status = MODEL_BAD_HASH_IDS;
  } else if (model->version() < 0 ||
             (model_.get() && model->version() < model_->version())) {
    model_status = MODEL_INVALID_VERSION_NUMBER;
  } else if (model_.get() && model->version() == model_->version()) {
    model_status = MODEL_NOT_CHANGED;
  } else {
    // The model is valid => replace the existing model with the new one.
    model_str_.assign(data);
    model_.swap(model);
    model_status = MODEL_SUCCESS;
  }
  EndFetch(model_status, max_age);
}

void ModelLoader::EndFetch(ClientModelStatus status, base::TimeDelta max_age) {
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());
  // We don't differentiate models in the UMA stats.
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ClientModelStatus",
                            status,
                            MODEL_STATUS_MAX);
  if (status == MODEL_SUCCESS) {
    update_renderers_callback_.Run();
  }
  int delay_ms = kClientModelFetchIntervalMs;
  // If the most recently fetched model had a valid max-age and the model was
  // valid we're scheduling the next model update for after the max-age expired.
  if (!max_age.is_zero() &&
      (status == MODEL_SUCCESS || status == MODEL_NOT_CHANGED)) {
    // We're adding 60s of additional delay to make sure we're past
    // the model's age.
    max_age += base::TimeDelta::FromMinutes(1);
    delay_ms = max_age.InMilliseconds();
  }

  // Reset |loader_| as it will be re-created on next load.
  url_loader_.reset();

  // Schedule the next model reload.
  ScheduleFetch(delay_ms);
}

void ModelLoader::ScheduleFetch(int64_t delay_ms) {
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ModelLoader::StartFetch, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void ModelLoader::CancelFetcher() {
  // This must be called on the same sequence as ScheduleFetch because it
  // invalidates any WeakPtrs allocated there.
  DCHECK(fetch_sequence_checker_.CalledOnValidSequence());
  // Invalidate any scheduled request.
  weak_factory_.InvalidateWeakPtrs();
  // Cancel any request in progress.
  url_loader_.reset();
}

}  // namespace safe_browsing

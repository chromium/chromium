// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/web_payments_observer.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/values.h"
#include "components/payments/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/third_party/mozilla/url_parse.h"

namespace payments {

namespace {

// Histogram and UKM names.
constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

// Query parameter keys.
constexpr char kChallengeRequestKey[] = "cReq";
constexpr char kChallengeResponseKey[] = "cRes";

ThreeDSecureTransactionStatus GetThreeDSecureTransactionStatus(
    std::string_view trans_status) {
  // TransStatus should always be a single character.
  if (trans_status.size() != 1) {
    return ThreeDSecureTransactionStatus::kUnknown;
  }

  switch (trans_status[0]) {
    case 'Y':
      return ThreeDSecureTransactionStatus::kSuccess;
    case 'N':
      return ThreeDSecureTransactionStatus::kDenied;
    case 'U':
      return ThreeDSecureTransactionStatus::kCouldNotBePerformed;
    case 'A':
      return ThreeDSecureTransactionStatus::kAttemptsProcessingPerformed;
    case 'C':
      return ThreeDSecureTransactionStatus::kChallengeRequired;
    case 'D':
      return ThreeDSecureTransactionStatus::kChallengeRequiredDecoupled;
    case 'R':
      return ThreeDSecureTransactionStatus::kRejected;
    case 'I':
      return ThreeDSecureTransactionStatus::kInformationalOnly;
    case 'S':
      return ThreeDSecureTransactionStatus::kChallengeUsingSPC;
    default:
      return ThreeDSecureTransactionStatus::kUnknown;
  }
}

ThreeDSecureTransactionStatus ParseChallengeResponse(
    std::string_view cres_val) {
  std::string decoded_json;
  std::string unescaped_cres = base::UnescapeBinaryURLComponent(cres_val);

  if (!base::Base64UrlDecode(unescaped_cres,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_json)) {
    // The `cres` value could be formatted as JWE (JSON Web Encryption)
    // which is distinguishable from the other formats by the presence
    // of periods in the string. In this case, we won't be able to parse
    // it, so we record it as encrypted JSON.
    return unescaped_cres.find('.') != std::string::npos
               ? ThreeDSecureTransactionStatus::kJSONEncrypted
               : ThreeDSecureTransactionStatus::kUnknown;
  }

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(decoded_json, base::JSON_PARSE_RFC);
  if (!dict) {
    return ThreeDSecureTransactionStatus::kUnknown;
  }

  const std::string* trans_status = dict->FindString("transStatus");
  if (!trans_status) {
    return ThreeDSecureTransactionStatus::kUnknown;
  }

  return GetThreeDSecureTransactionStatus(*trans_status);
}

}  // namespace

WebPaymentsObserver::WebPaymentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebPaymentsObserver::~WebPaymentsObserver() = default;

void WebPaymentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kThreeDSecureTelemetry)) {
    RecordThreeDSecureTelemetry(navigation_handle);
  }
}

void WebPaymentsObserver::RecordThreeDSecureTelemetry(
    content::NavigationHandle* navigation_handle) {
  // Filter for only HTTP POST requests from form submissions.
  if (!navigation_handle->IsPost() || !navigation_handle->IsFormSubmission()) {
    return;
  }

  scoped_refptr<const network::ResourceRequestBody> post_data =
      navigation_handle->GetPostData();
  if (!post_data || !post_data->elements()) {
    return;
  }

  for (const network::DataElement& element : *post_data->elements()) {
    if (const auto* bytes_element =
            element.TryAs<network::DataElementBytes>()) {
      std::string_view body = bytes_element->AsStringView();
      url::Component query(body);
      url::Component key;
      url::Component value;
      while (url::ExtractQueryKeyValue(body, &query, &key, &value)) {
        std::string_view key_view = key.AsViewOn(body);
        if (base::EqualsCaseInsensitiveASCII(key_view, kChallengeRequestKey)) {
          base::UmaHistogramBoolean(kChallengeRequestHistogram, true);
          ukm::builders::Payments_ThreeDSecure_ChallengeRequest(
              navigation_handle->GetNextPageUkmSourceId())
              .SetChallengeRequest(true)
              .Record(ukm::UkmRecorder::Get());
        } else if (base::EqualsCaseInsensitiveASCII(key_view,
                                                    kChallengeResponseKey)) {
          ThreeDSecureTransactionStatus result =
              ParseChallengeResponse(value.AsViewOn(body));
          base::UmaHistogramEnumeration(kChallengeResponseHistogram, result);
          ukm::builders::Payments_ThreeDSecure_ChallengeResponse(
              navigation_handle->GetNextPageUkmSourceId())
              .SetChallengeResponse(static_cast<int64_t>(result))
              .Record(ukm::UkmRecorder::Get());
        }
      }
    }
  }
}

}  // namespace payments

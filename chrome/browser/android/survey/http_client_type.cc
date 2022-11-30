// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/survey/http_client_type.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace survey {
namespace {
constexpr char kResponseCodeHistogramPrefix[] =
    "Net.HttpResponseCode.CustomHttpClient.";

constexpr net::NetworkTrafficAnnotationTag kHatsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_android_hats", R"(
        semantics {
          sender: "Chrome HaTS Next Service"
          description:
            "Chrome use HaTS to collect user feedback in the form of surveys. "
            "For eligible users, a survey invitation will be presented and the "
            "user can choose to take the survey or dismiss it."
          trigger:
            "If a signed-in user is eligible and selected for a survey, a "
            "request will be sent to download the survey; requests will "
            "also be sent when the user chooses to take the survey and "
            "for responses to each question to record such user actions."
          data:
            "Survey questions to present to the user; subsequently, survey "
            "visibility ack and responses to questions using session context "
            "from the initial trigger request. All requests have auth token "
            "tied to signed-in GAIA account."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only enabled when UMA and crash reporting is "
            "enabled (configurable in settings)."
          policy_exception_justification:
            "UMA and crash reporting is already controllable via Enterprise "
            "policy."
        })");

constexpr net::NetworkTrafficAnnotationTag kChimeTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chime_sdk", R"(
        semantics {
          sender: "Chrome Chime notification SDK"
          description:
            "The custom network library used in Chime SDK."
          trigger:
            "when the user receives notification or interacts with the"
            "notification, and when Chrome is launched in the foreground."
          data:
            "Registration data, Chime application data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Currently there is no setting control. When all the features"
            "using Chime are disabled, Chime is disabled."
          policy_exception_justification:
            "Not implemented."
        })");

}  // namespace

net::NetworkTrafficAnnotationTag GetTrafficAnnotation(
    HttpClientType client_type) {
  switch (client_type) {
    case HttpClientType::kSurvey:
      return kHatsTrafficAnnotation;
    case HttpClientType::kNotification:
      return kChimeTrafficAnnotation;
  }
}

void RecordHttpResponseCodeHistogram(HttpClientType client_type,
                                     int response_code) {
  std::string histogram_suffix;
  switch (client_type) {
    case HttpClientType::kSurvey:
      histogram_suffix = "Survey";
      break;
    case HttpClientType::kNotification:
      histogram_suffix = "Notification";
      break;
  }
  std::string histogram_name =
      base::StrCat({kResponseCodeHistogramPrefix, histogram_suffix});
  int status_code = net::HttpUtil::MapStatusCodeForHistogram(response_code);
  base::UmaHistogramSparse(histogram_name, status_code);
}

}  // namespace survey

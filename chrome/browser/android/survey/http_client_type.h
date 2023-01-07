// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_
#define CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace survey {

// Defines different types of consumers of SurveyHttpClient. Each consumer type
// will have different network annotation. A Java counterpart will be generated
// for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.survey
enum class HttpClientType {
  kSurvey = 0,
  kNotification = 1,
};

// Get the traffic annotations corresponding to |client_type|.
net::NetworkTrafficAnnotationTag GetTrafficAnnotation(
    HttpClientType client_type);

// Record the |response_code| for histogram
// "Net.HttpResponseCode.CustomHttpClient.*" based on the |client_type|.
void RecordHttpResponseCodeHistogram(HttpClientType client_type,
                                     int response_code);

}  // namespace survey

#endif  // CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_

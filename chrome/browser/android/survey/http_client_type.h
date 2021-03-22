// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_
#define CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_

namespace survey {

// Defines different types of consumers of SurveyHttpClient. Each consumer type
// will have different network annotation. A Java counterpart will be generated
// for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.survey
enum class HttpClientType {
  kSurvey = 0,
  kNotification = 1,
};

}  // namespace survey

#endif  // CHROME_BROWSER_ANDROID_SURVEY_HTTP_CLIENT_TYPE_H_

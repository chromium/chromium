// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_ANDROID_APP_INFO_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_ANDROID_APP_INFO_GENERATOR_H_

#include <memory>
#include <string>

class ArcAppListPrefs;

namespace enterprise_management {
class AndroidAppInfo;
}  // namespace enterprise_management

namespace enterprise_reporting {

// A class that is responsible for collecting Android application information
// with given |app_id|.
class AndroidAppInfoGenerator {
 public:
  AndroidAppInfoGenerator() = default;
  AndroidAppInfoGenerator(const AndroidAppInfoGenerator&) = delete;
  AndroidAppInfoGenerator& operator=(const AndroidAppInfoGenerator&) = delete;
  ~AndroidAppInfoGenerator() = default;

  std::unique_ptr<enterprise_management::AndroidAppInfo> Generate(
      ArcAppListPrefs* prefs,
      const std::string& app_id) const;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_ANDROID_APP_INFO_GENERATOR_H_

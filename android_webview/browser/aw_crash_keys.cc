// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_crash_keys.h"

#include <deque>
#include <string_view>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"

namespace android_webview {

namespace {

// A convenient wrapper around a crash key and its name.
// Lifted from chrome/common
class CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string name)
      : name_(std::move(name)), crash_key_(name_.c_str()) {}
  CrashKeyWithName(const CrashKeyWithName&) = delete;
  CrashKeyWithName& operator=(const CrashKeyWithName&) = delete;
  CrashKeyWithName(CrashKeyWithName&&) = delete;
  CrashKeyWithName& operator=(CrashKeyWithName&&) = delete;
  ~CrashKeyWithName() = delete;

  void Clear() { crash_key_.Clear(); }
  void Set(std::string_view value) { crash_key_.Set(value); }

 private:
  std::string name_;
  crash_reporter::CrashKeyString<64> crash_key_;
};

}  // namespace

void SetCrashKeysFromFeaturesAndSwitches(
    const std::set<std::string>& switches,
    const std::set<std::string>& features) {
  static base::NoDestructor<std::deque<CrashKeyWithName>> runtime_crash_keys;
  static size_t crash_id = 0;
  static size_t enabled_features_count = 0;
  static size_t disabled_features_count = 0;
  static size_t switches_count = 0;

  for (auto feature : features) {
    size_t position_of_state = feature.find_last_of(":");

    std::string value = feature.substr(0, position_of_state);
    std::string enabled_state = feature.substr(position_of_state + 1);

    size_t feature_count = enabled_state == "enabled"
                               ? ++enabled_features_count
                               : ++disabled_features_count;

    runtime_crash_keys->emplace_back(base::StringPrintf(
        "commandline-%s-feature-%zu", enabled_state.c_str(), feature_count));
    (*runtime_crash_keys)[crash_id++].Set(value);
  }

  for (auto value : switches) {
    runtime_crash_keys->emplace_back(
        base::StringPrintf("switch-%zu", ++switches_count));
    (*runtime_crash_keys)[crash_id++].Set(value);
  }

  static crash_reporter::CrashKeyString<4> num_switches("num-switches");
  num_switches.Set(base::NumberToString(switches_count));
}

}  // namespace android_webview

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/metrics/app_package_name_logging_rule.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android_webview {

namespace {
constexpr char kExpiryDateKey[] = "expiry-date";
constexpr char kVersionKey[] = "allowlist-version";
}  // namespace

AppPackageNameLoggingRule::AppPackageNameLoggingRule(
    const base::Version& version,
    const base::Time& expiry_date)
    : version_(version), expiry_date_(expiry_date) {
  DCHECK(version.IsValid());
  DCHECK(!expiry_date.is_null());
}

base::Version AppPackageNameLoggingRule::GetVersion() const {
  return version_;
}

base::Time AppPackageNameLoggingRule::GetExpiryDate() const {
  return expiry_date_;
}

bool AppPackageNameLoggingRule::IsAppPackageNameAllowed() const {
  return expiry_date_ >= base::Time::Now();
}

bool AppPackageNameLoggingRule::IsSameAs(
    const AppPackageNameLoggingRule& record) const {
  if (record.GetVersion() == GetVersion()) {
    DCHECK(record.GetExpiryDate() == GetExpiryDate());
    return true;
  }
  return false;
}

// static
absl::optional<AppPackageNameLoggingRule>
AppPackageNameLoggingRule::FromDictionary(const base::Value::Dict& dict) {
  const std::string* version_string = dict.FindString(kVersionKey);
  if (!version_string) {
    return absl::optional<AppPackageNameLoggingRule>();
  }
  base::Version version(*version_string);
  if (!version.IsValid()) {
    return absl::optional<AppPackageNameLoggingRule>();
  }

  const base::Value* expiry_date_value = dict.Find(kExpiryDateKey);
  if (!expiry_date_value) {
    return AppPackageNameLoggingRule(version, base::Time::Min());
  }
  absl::optional<base::Time> expiry_date =
      base::ValueToTime(*expiry_date_value);
  if (!expiry_date.has_value()) {
    return AppPackageNameLoggingRule(version, base::Time::Min());
  }

  return AppPackageNameLoggingRule(version, expiry_date.value());
}

base::Value::Dict AppPackageNameLoggingRule::ToDictionary() {
  base::Value::Dict dict;

  dict.Set(kVersionKey, version_.GetString());
  if (!expiry_date_.is_min()) {
    dict.Set(kExpiryDateKey, base::TimeToValue(expiry_date_));
  }
  return dict;
}

}  // namespace android_webview

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_SERVICE_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_SERVICE_SETTINGS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// The settings for a report service obtained from a connector policy.
class ReportingServiceSettings {
 public:
  explicit ReportingServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  ReportingServiceSettings(ReportingServiceSettings&&);
  ~ReportingServiceSettings();

  // Get the settings to apply to a specific report. absl::nullopt implies no
  // report should take place.
  absl::optional<ReportingSettings> GetReportingSettings() const;

  std::string service_provider_name() const { return service_provider_name_; }

 private:
  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetAnalysisSettings will always return absl::nullopt.
  bool IsValid() const;

  // The service provider matching the name given in a Connector policy. nullptr
  // implies that a corresponding service provider doesn't exist and that these
  // settings are not valid.
  raw_ptr<const ServiceProviderConfig::ServiceProvider> service_provider_ =
      nullptr;

  std::string service_provider_name_;

  // The events that are enabled for the current service provider.
  std::set<std::string> enabled_event_names_;

  // The enabled opt-in events for the current service provider, mapping to the
  // URL patterns that represent on which URL they are enabled.
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_SERVICE_SETTINGS_H_

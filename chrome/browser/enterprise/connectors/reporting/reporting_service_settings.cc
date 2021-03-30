// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

namespace {

constexpr char kReportingConnectorUrlFlag[] = "reporting-connector-url";

base::Optional<GURL> GetUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return base::nullopt;
  }

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(kReportingConnectorUrlFlag)) {
    GURL url = GURL(cmd->GetSwitchValueASCII(kReportingConnectorUrlFlag));
    if (url.is_valid())
      return url;
    else
      VLOG(1) << "--reporting-connector-url is set to an invalid URL";
  }

  return base::nullopt;
}

}  // namespace

ReportingServiceSettings::ReportingServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_name_ = *service_provider_name;
    service_provider_ =
        service_provider_config.GetServiceProvider(*service_provider_name);
  }

  const base::Value* enabled_event_name_list_value =
      settings_value.FindListKey(kKeyEnabledEventNames);
  if (enabled_event_name_list_value) {
    for (const base::Value& enabled_event_name_value :
         enabled_event_name_list_value->GetList()) {
      if (enabled_event_name_value.is_string())
        enabled_event_names_.insert(enabled_event_name_value.GetString());
      else
        DVLOG(1) << "Enabled event name list contains a non string value!";
    }
  } else {
    // When the list of enabled event names is not set, we assume all events are
    // enabled. This is to support the feature of selecting the "All always on"
    // option in the policy UI, which means to always enable all events, even
    // when new events may be added in the future. And this is also to support
    // existing customer policies that were created before we introduced the
    // concept of enabling/disabling events.
    for (auto* event_name :
         extensions::SafeBrowsingPrivateEventRouter::kAllEvents) {
      enabled_event_names_.insert(event_name);
    }
  }
}

base::Optional<ReportingSettings>
ReportingServiceSettings::GetReportingSettings() const {
  if (!IsValid())
    return base::nullopt;

  ReportingSettings settings;

  settings.reporting_url =
      GetUrlOverride().value_or(GURL(service_provider_->reporting_url()));
  DCHECK(settings.reporting_url.is_valid());

  settings.enabled_event_names.insert(enabled_event_names_.begin(),
                                      enabled_event_names_.end());
  return settings;
}

bool ReportingServiceSettings::IsValid() const {
  // The settings are valid only if a provider was given, and events are
  // enabled. The list could be empty. The absence of a list means "all events",
  // but the presence of an empty list means "no events".
  return service_provider_ && !enabled_event_names_.empty();
}

ReportingServiceSettings::ReportingServiceSettings(ReportingServiceSettings&&) =
    default;
ReportingServiceSettings::~ReportingServiceSettings() = default;

}  // namespace enterprise_connectors

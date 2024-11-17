// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/features.h"
#endif

namespace enterprise_connectors {

namespace {

bool IsManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS)
  return chromeos::IsManagedGuestSession();
#else
  return false;
#endif
}
}  // namespace

RealtimeReportingClient::RealtimeReportingClient(
    content::BrowserContext* context)
    : RealtimeReportingClientBase(
          g_browser_process->browser_policy_connector()
              ->device_management_service(),
          g_browser_process->shared_url_loader_factory()),
      context_(context) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
}

RealtimeReportingClient::~RealtimeReportingClient() = default;

bool RealtimeReportingClient::ShouldInitRealtimeReportingClient() {
  if (IsManagedGuestSession() &&
      !base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)) {
    DVLOG(2) << "Safe browsing real-time reporting is not enabled in Managed "
                "Guest Sessions.";
    return false;
  }

  return true;
}

void RealtimeReportingClient::SetBrowserCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && browser_client_)
    browser_client_->RemoveObserver(this);

  browser_client_ = client;
  if (browser_client_)
    browser_client_->AddObserver(this);
}

void RealtimeReportingClient::SetProfileCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && profile_client_)
    profile_client_->RemoveObserver(this);

  profile_client_ = client;
  if (profile_client_)
    profile_client_->AddObserver(this);
}

void RealtimeReportingClient::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::pair<std::string, policy::CloudPolicyClient*>
RealtimeReportingClient::InitProfileReportingClient(
    const std::string& dm_token) {
  policy::CloudPolicyManager* policy_manager =
      Profile::FromBrowserContext(context_)->GetCloudPolicyManager();
  if (!policy_manager || !policy_manager->core() ||
      !policy_manager->core()->client()) {
    return {GetProfilePolicyClientDescription(), nullptr};
  }

  profile_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      policy_manager->core()->client()->service(),
      g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  policy::CloudPolicyClient* client = profile_private_client_.get();

  client->SetupRegistration(dm_token,
                            policy_manager->core()->client()->client_id(),
                            /*user_affiliation_ids*/ {});

  return {GetProfilePolicyClientDescription(), client};
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

std::optional<ReportingSettings>
RealtimeReportingClient::GetReportingSettings() {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(context_);
  if (!service) {
    return std::nullopt;
  }

  return service->GetReportingSettings();
}

void RealtimeReportingClient::ReportRealtimeEvent(
    const std::string& name,
    const ReportingSettings& settings,
    base::Value::Dict event) {
  ReportEventWithTimestamp(name, settings, std::move(event), base::Time::Now(),
                           /*include_profile_user_name=*/true);
}

void RealtimeReportingClient::ReportPastEvent(const std::string& name,
                                              const ReportingSettings& settings,
                                              base::Value::Dict event,
                                              const base::Time& time) {
  // Do not include profile information for past events because for crash events
  // we do not necessarily know which profile caused the crash .
  ReportEventWithTimestamp(name, settings, std::move(event), time,
                           /*include_profile_user_name=*/false);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void AddCrowdstrikeSignalsToEvent(
    base::Value::Dict& event,
    const device_signals::SignalsAggregationResponse& response) {
  if (!response.agent_signals_response ||
      !response.agent_signals_response->crowdstrike_signals) {
    return;
  }
  const auto& crowdstrike_signals =
      response.agent_signals_response->crowdstrike_signals.value();

  base::Value::Dict crowdstrike_agent_fields;
  crowdstrike_agent_fields.Set("agent_id", crowdstrike_signals.agent_id);
  crowdstrike_agent_fields.Set("customer_id", crowdstrike_signals.customer_id);
  base::Value::Dict crowdstrike_agent;
  crowdstrike_agent.Set("crowdstrike", std::move(crowdstrike_agent_fields));
  base::Value::List agents;
  agents.Append(std::move(crowdstrike_agent));
  event.Set("securityAgents", std::move(agents));
}

#endif

void RealtimeReportingClient::SetProfileUserNameForTesting(
    std::string username) {
  username_ = std::move(username);
}

std::string RealtimeReportingClient::GetProfileUserName() {
  if (!username_.empty()) {
    return username_;
  }
  username_ =
      identity_manager_ ? GetProfileEmail(identity_manager_) : std::string();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (username_.empty()) {
    username_ = Profile::FromBrowserContext(context_)->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return username_;
}

std::string RealtimeReportingClient::GetProfileIdentifier() {
  if (profile_client_) {
    auto* profile_id_service =
        enterprise::ProfileIdServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context_));
    if (profile_id_service && profile_id_service->GetProfileId().has_value()) {
      return profile_id_service->GetProfileId().value();
    }
    return std::string();
  }

  return Profile::FromBrowserContext(context_)->GetPath().AsUTF8Unsafe();
}

std::string RealtimeReportingClient::GetBrowserClientId() {
  std::string client_id;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = nullptr;
  const user_manager::User* user = GetChromeOSUser();
  if (user) {
    profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    // If primary user profile is not finalized, use the current profile.
    if (!profile) {
      profile = Profile::FromBrowserContext(context_);
    }
  } else {
    LOG(ERROR) << "Could not determine who the user is.";
    profile = Profile::FromBrowserContext(context_);
  }
  DCHECK(profile);

  if (IsManagedGuestSession()) {
    client_id = reporting::GetMGSUserClientId().value_or("");
  } else {
    client_id = reporting::GetUserClientId(profile).value_or("");
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* main_profile = GetMainProfileLacros();
  if (main_profile) {
    // Prefer the user client id if available.
    client_id = reporting::GetUserClientId(main_profile).value_or(client_id);
  }
#else
  client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#endif
  return client_id;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void RealtimeReportingClient::MaybeCollectDeviceSignalsAndReportEvent(
    base::Value::Dict event,
    policy::CloudPolicyClient* client,
    std::string name,
    const ReportingSettings& settings,
    base::Time time) {
  Profile* profile = Profile::FromBrowserContext(context_);
  device_signals::SignalsAggregator* signals_aggregator =
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile);
  if (signals_aggregator) {
    device_signals::SignalsAggregationRequest request;
    request.signal_names.emplace(device_signals::SignalName::kAgent);
    signals_aggregator->GetSignals(
        request,
        base::BindOnce(&RealtimeReportingClient::PopulateSignalsAndReportEvent,
                       AsWeakPtrImpl(), std::move(event), client, name,
                       settings, context_, time));
  } else {
    UploadSecurityEventReport(std::move(event), client, name, settings, time);
  }
}

void RealtimeReportingClient::PopulateSignalsAndReportEvent(
    base::Value::Dict event,
    policy::CloudPolicyClient* client,
    std::string name,
    ReportingSettings settings,
    content::BrowserContext* context,
    base::Time time,
    device_signals::SignalsAggregationResponse response) {
  AddCrowdstrikeSignalsToEvent(event, response);
  UploadSecurityEventReport(std::move(event), client, name, settings, time);
}
#endif

bool RealtimeReportingClient::ShouldIncludeDeviceInfo(bool per_profile) {
  return IncludeDeviceInfo(Profile::FromBrowserContext(context_), per_profile);
}

void RealtimeReportingClient::UploadCallback(
    base::Value::Dict event_wrapper,
    bool per_profile,
    policy::CloudPolicyClient* client,
    EnterpriseReportingEventType eventType,
    policy::CloudPolicyClient::Result upload_result) {
  // TODO(crbug.com/256553070): Do not crash if the client is unregistered.
  CHECK(!upload_result.IsClientNotRegisteredError());

// Device DM token is already set on Ash by reporting::GetContext(...)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!per_profile && client) {
    event_wrapper.SetByDottedPath(
        "context.device",
        policy::ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            BuildDeviceDictionary(client->dm_token(), client->client_id()));
  }
#endif
  event_wrapper.Set("uploaded_successfully", upload_result.IsSuccess());

  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
      std::move(event_wrapper));

  if (upload_result.IsSuccess()) {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadSuccess",
                                  eventType);
  } else {
    base::UmaHistogramEnumeration("Enterprise.ReportingEventUploadFailure",
                                  eventType);
  }
}

base::Value::Dict RealtimeReportingClient::GetContext() {
  return reporting::GetContext(Profile::FromBrowserContext(context_));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
const user_manager::User* RealtimeReportingClient::GetChromeOSUser() {
  return user_manager::UserManager::IsInitialized()
             ? user_manager::UserManager::Get()->GetPrimaryUser()
             : nullptr;
}

#endif

void RealtimeReportingClient::RemoveDmTokenFromRejectedSet(
    const std::string& dm_token) {
  rejected_dm_token_timers_.erase(dm_token);
}

void RealtimeReportingClient::OnClientError(policy::CloudPolicyClient* client) {
  base::Value::Dict error_value;
  error_value.Set("error",
                  "An event got an error status and hasn't been reported");
  error_value.Set("status", client->last_dm_status());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
      error_value);

  // This is the status set when the server returned 403, which is what the
  // reporting server returns when the customer is not allowed to report events.
  if (client->last_dm_status() ==
      policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
    // This could happen if a second event was fired before the first one
    // returned an error.
    if (!rejected_dm_token_timers_.contains(client->dm_token())) {
      rejected_dm_token_timers_[client->dm_token()] =
          std::make_unique<base::OneShotTimer>();
      rejected_dm_token_timers_[client->dm_token()]->Start(
          FROM_HERE, base::Hours(24),
          base::BindOnce(&RealtimeReportingClient::RemoveDmTokenFromRejectedSet,
                         AsWeakPtrImpl(), client->dm_token()));
    }
  }
}

base::WeakPtr<RealtimeReportingClientBase>
RealtimeReportingClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors

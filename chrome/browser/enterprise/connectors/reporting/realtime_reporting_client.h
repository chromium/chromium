// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/signals_types.h"
#endif

namespace content {
class BrowserContext;
}

namespace signin {
class IdentityManager;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace user_manager {
class User;
}

#endif

namespace enterprise_connectors {

// An event router that observes Safe Browsing events and notifies listeners.
// The router also uploads events to the chrome reporting server side API if
// the kRealtimeReportingFeature feature is enabled.
class RealtimeReportingClient : public KeyedService,
                                public policy::CloudPolicyClient::Observer {
 public:
  static const char kKeyProfileIdentifier[];
  static const char kKeyProfileUserName[];

  explicit RealtimeReportingClient(content::BrowserContext* context);

  RealtimeReportingClient(const RealtimeReportingClient&) = delete;
  RealtimeReportingClient& operator=(const RealtimeReportingClient&) = delete;

  ~RealtimeReportingClient() override;

  // Returns true if enterprise real-time reporting should be initialized,
  // checking both the feature flag. This function is public so that it can
  // called in tests.
  static bool ShouldInitRealtimeReportingClient();

  void SetBrowserCloudPolicyClientForTesting(policy::CloudPolicyClient* client);
  void SetProfileCloudPolicyClientForTesting(policy::CloudPolicyClient* client);

  void SetIdentityManagerForTesting(signin::IdentityManager* identity_manager);

  // policy::CloudPolicyClient::Observer:
  void OnClientError(policy::CloudPolicyClient* client) override;
  void OnPolicyFetched(policy::CloudPolicyClient* client) override {}
  void OnRegistrationStateChanged(policy::CloudPolicyClient* client) override {}

  base::WeakPtr<RealtimeReportingClient> GetWeakPtr();

  // Determines if the real-time reporting feature is enabled.
  // Obtain settings to apply to a reporting event from ConnectorsService.
  // std::nullopt represents that reporting should not be done.
  // Declared virtual for tests.
  std::optional<ReportingSettings> virtual GetReportingSettings();

  // Returns the Gaia email address of the account signed in to the profile or
  // an empty string if the profile is not signed in (declared virtual for
  // tests).
  virtual std::string GetProfileUserName() const;

  // Report safe browsing event through real-time reporting channel, if enabled.
  // Declared as virtual for tests.
  virtual void ReportRealtimeEvent(const std::string&,
                                   const ReportingSettings& settings,
                                   base::Value::Dict event);

  // Report safe browsing events that have occurred in the past but has not yet
  // been reported. This is currently used for browser crash events, which are
  // polled at a fixed time interval. Declared as virtual for tests.
  virtual void ReportPastEvent(const std::string&,
                               const ReportingSettings& settings,
                               base::Value::Dict event,
                               const base::Time& time);

 private:
  // Initialize a real-time report client if needed.  This client is used only
  // if real-time reporting is enabled, the machine is properly reigistered
  // with CBCM and the appropriate policies are enabled.
  void InitRealtimeReportingClient(const ReportingSettings& settings);

  // Helper function that uploads security events, parametrized with the time.
  void ReportEventWithTimestamp(const std::string& name,
                                const ReportingSettings& settings,
                                base::Value::Dict event,
                                const base::Time& time,
                                bool include_profile_user_name);

  // Returns the profile identifier which is the path to the current profile on
  // managed browsers or the globally unique profile identifier otherwise.
  std::string GetProfileIdentifier() const;

  // Sub-methods called by InitRealtimeReportingClient to make appropriate
  // verifications and initialize the corresponding client. Returns a policy
  // client description and a client, which can be nullptr if it can't be
  // initialized.
  std::pair<std::string, policy::CloudPolicyClient*> InitBrowserReportingClient(
      const std::string& dm_token);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::pair<std::string, policy::CloudPolicyClient*> InitProfileReportingClient(
      const std::string& dm_token);
#endif

  // Handle the availability of a cloud policy client.
  void OnCloudPolicyClientAvailable(const std::string& policy_client_desc,
                                    policy::CloudPolicyClient* client);

#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Return the Chrome OS user who is subject to reporting, or nullptr if
  // the user cannot be deterined.
  static const user_manager::User* GetChromeOSUser();

#endif

  void RemoveDmTokenFromRejectedSet(const std::string& dm_token);

  raw_ptr<content::BrowserContext> context_;
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_ =
      nullptr;

  // The cloud policy clients used to upload browser events and profile events
  // to the cloud. These clients are never used to fetch policies. These
  // pointers are not owned by the class.
  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> browser_client_ =
      nullptr;
  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> profile_client_ =
      nullptr;

  // The private clients are used on platforms where we cannot just get a
  // client and we create our own (used through the above client pointers).
  std::unique_ptr<policy::CloudPolicyClient> browser_private_client_;
  std::unique_ptr<policy::CloudPolicyClient> profile_private_client_;

  // When a request is rejected for a given DM token, wait 24 hours before
  // trying again for this specific DM Token.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      rejected_dm_token_timers_;

  base::WeakPtrFactory<RealtimeReportingClient> weak_ptr_factory_{this};
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Populate event dict with CrowdStrike signal values. If those signals are
// available in `response`, this function returns a Dict with the following
// fields added:
// "securityAgents" : [
//   {
//     "crowdstrike": {
//       "agent_id": "agent-123",
//       "customer_id": "customer-123"
//     }
//   }
// ]
// These must match proto specified in
// chrome/cros/reporting/api/proto/browser_events.proto
void AddCrowdstrikeSignalsToEvent(
    base::Value::Dict& event,
    const device_signals::SignalsAggregationResponse& response);
#endif

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_H_

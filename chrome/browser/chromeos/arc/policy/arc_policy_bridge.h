// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/arc/mojom/policy.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class BrowserContextKeyedServiceFactory;

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {
class PolicyMap;
}  // namespace policy

namespace arc {

class ArcBridgeService;

// Constants for the ARC certs sync mode are defined in the policy, please keep
// its in sync.
enum ArcCertsSyncMode : int32_t {
  // Certificates sync is disabled.
  SYNC_DISABLED = 0,
  // Copy of CA certificates is enabled.
  COPY_CA_CERTS = 1
};

class ArcPolicyBridge : public KeyedService,
                        public ConnectionObserver<mojom::PolicyInstance>,
                        public mojom::PolicyHost,
                        public policy::PolicyService::Observer {
 public:
  class Observer {
   public:
    // Called when policy is sent to CloudDPC.
    virtual void OnPolicySent(const std::string& policy) {}

    // Called when a compliance report is received from CloudDPC.
    virtual void OnComplianceReportReceived(
        const base::Value* compliance_report) {}

    // Called when a request to install set of packages was sent to CloudDPS.
    virtual void OnCloudDpsRequested(
        base::Time time,
        const std::set<std::string>& package_names) {}

    // Called when CloudDPS successfully processed request for install for a
    // set of packages. Note |package_names| may not match to what was
    // requested.
    virtual void OnCloudDpsSucceeded(
        base::Time time,
        const std::set<std::string>& package_names) {}

    // Called when CloudDPS returned an error for the package installation
    // request. |reason| defines the failure reason.
    virtual void OnCloudDpsFailed(base::Time time,
                                  const std::string& package_name,
                                  mojom::InstallErrorReason reason) {}

    // Called when CloudDPC scheduled direct install with Play Store for
    // a set of packages.
    virtual void OnReportDirectInstall(
        base::Time time,
        const std::set<std::string>& package_names) {}

    // Called when in CloudDPC the main loop of retries to install apps failed
    // to install some apps.
    virtual void OnReportForceInstallMainLoopFailed(
        base::Time time,
        const std::set<std::string>& package_names) {}

   protected:
    Observer() = default;
    virtual ~Observer() = default;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPolicyBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcPolicyBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Return the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  base::WeakPtr<ArcPolicyBridge> GetWeakPtr();

  ArcPolicyBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ArcPolicyBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service,
                  policy::PolicyService* policy_service);
  ~ArcPolicyBridge() override;

  const std::string& GetInstanceGuidForTesting();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OverrideIsManagedForTesting(bool is_managed);

  // ConnectionObserver<mojom::PolicyInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // PolicyHost overrides.
  void GetPolicies(GetPoliciesCallback callback) override;
  void ReportCompliance(const std::string& request,
                        ReportComplianceCallback callback) override;
  void ReportCloudDpsRequested(
      base::Time time,
      const std::vector<std::string>& package_names) override;
  void ReportCloudDpsSucceeded(
      base::Time time,
      const std::vector<std::string>& package_names) override;
  void ReportCloudDpsFailed(base::Time time,
                            const std::string& package_name,
                            mojom::InstallErrorReason reason) override;
  void ReportDirectInstall(
      base::Time time,
      const std::vector<std::string>& package_names) override;
  void ReportForceInstallMainLoopFailed(
      base::Time time,
      const std::vector<std::string>& package_names) override;

  // PolicyService::Observer overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void OnCommandReceived(
      const std::string& command,
      mojom::PolicyInstance::OnCommandReceivedCallback callback);

 private:
  void InitializePolicyService();

  // Returns the current policies for ARC, in the JSON dump format.
  std::string GetCurrentJSONPolicies() const;

  // Called when the compliance report from ARC is parsed.
  void OnReportComplianceParse(
      base::OnceCallback<void(const std::string&)> callback,
      data_decoder::DataDecoder::ValueOrError result);

  void UpdateComplianceReportMetrics(const base::DictionaryValue* report);

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  policy::PolicyService* policy_service_ = nullptr;

  bool is_managed_ = false;

  // HACK(b/73762796): A GUID that is regenerated whenever |this| is created,
  // ensuring that the first policy sent to CloudDPC is considered different
  // from previous policy and a compliance report is sent.
  const std::string instance_guid_;
  // Hash of the policies that were up to date when ARC started.
  std::string initial_policies_hash_;
  // Whether the UMA metric for the first successfully obtained compliance
  // report was already reported.
  bool first_compliance_timing_reported_ = false;
  // Hash of the policies that were up to date when the most recent policy
  // update notification was sent to ARC.
  std::string update_notification_policies_hash_;
  // The time of the policy update notification sent when the policy with hash
  // equal to |update_notification_policy_hash_| was active.
  base::TimeTicks update_notification_time_;
  // Whether the UMA metric for the successfully obtained compliance report
  // since the most recent policy update notificaton was already reported.
  bool compliance_since_update_timing_reported_ = false;

  base::ObserverList<Observer, true /* check_empty */>::Unchecked observers_;

  // Called when the ARC connection is ready.
  base::OnceClosure on_arc_instance_ready_callback_;

  // Must be the last member.
  base::WeakPtrFactory<ArcPolicyBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_BRIDGE_H_

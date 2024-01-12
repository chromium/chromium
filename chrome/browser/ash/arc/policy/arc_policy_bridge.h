// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_BRIDGE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/policy.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
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
                        public policy::PolicyService::Observer,
                        public arc::ArcSessionManagerObserver {
 public:
  class Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when policy is sent to CloudDPC.
    virtual void OnPolicySent(const std::string& policy) {}

    // Called when a compliance report is received from CloudDPC.
    virtual void OnComplianceReportReceived(
        const base::Value* compliance_report) {}

    // Called when ARC DPC starts.
    virtual void OnReportDPCVersion(const std::string& version) {}

    // Called when a Play Store local policy was set.
    virtual void OnPlayStoreLocalPolicySet(
        base::Time time,
        const std::set<std::string>& package_names) {}

   protected:
    Observer() = default;
    virtual ~Observer() = default;
  };

  // Policy constants.
  static const char kApplications[];
  static const char kPackageName[];
  static const char kManagedConfiguration[];

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

  ArcPolicyBridge(const ArcPolicyBridge&) = delete;
  ArcPolicyBridge& operator=(const ArcPolicyBridge&) = delete;

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
  void ReportDPCVersion(const std::string& version) override;
  void ReportPlayStoreLocalPolicySet(
      base::Time time,
      const std::vector<std::string>& package_names) override;

  // PolicyService::Observer overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // arc::ArcSessionManagerObserver overrides
  void OnArcStartDelayed() override;

  void OnCommandReceived(
      const std::string& command,
      mojom::PolicyInstance::OnCommandReceivedCallback callback);

  const std::string& get_arc_policy_for_reporting() {
    return arc_policy_for_reporting_;
  }
  const std::string& get_arc_policy_compliance_report() {
    return arc_policy_compliance_report_;
  }
  const std::string& get_arc_dpc_version() { return arc_dpc_version_; }

  static void EnsureFactoryBuilt();

 private:
  void InitializePolicyService();

  // Returns the current policies for ARC, in the JSON dump format.
  std::string GetCurrentJSONPolicies() const;

  // Called when the compliance report from ARC is parsed.
  void OnReportComplianceParse(
      base::OnceCallback<void(const std::string&)> callback,
      data_decoder::DataDecoder::ValueOrError result);

  // Check the policy to see if ARC needs to be activated to install any
  // applications
  static void ActivateArcIfRequiredByPolicy(
      const policy::PolicyMap& policy_map);

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  raw_ptr<policy::PolicyService> policy_service_ = nullptr;

  bool is_managed_ = false;
  bool is_policy_service_observed = false;

  // HACK(b/73762796): A GUID that is regenerated whenever |this| is created,
  // ensuring that the first policy sent to CloudDPC is considered different
  // from previous policy and a compliance report is sent.
  const std::string instance_guid_;

  base::ObserverList<Observer, true /* check_empty */>::Unchecked observers_;

  // Called when the ARC connection is ready.
  base::OnceClosure on_arc_instance_ready_callback_;

  // Saved last sent ArcPolicy. Should be used only for feedback reporting.
  std::string arc_policy_for_reporting_;
  // Saved last received compliance report. Should be used only for feedback
  // reporting.
  std::string arc_policy_compliance_report_;
  // Saved ARC DPC version.
  std::string arc_dpc_version_;

  // Must be the last member.
  base::WeakPtrFactory<ArcPolicyBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_BRIDGE_H_

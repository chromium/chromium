// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;
class GURL;

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace enterprise_connectors {

// Keyed-Service in charge of monitoring the status of the Device Trust
// connector (e.g. enabled or not).
class DeviceTrustConnectorService : public KeyedService {
 public:
  // Classes extending this class can be added as observers to Device Trust
  // connector status changes.
  class PolicyObserver {
   public:
    friend std::default_delete<PolicyObserver>;
    virtual void OnInlinePolicyEnabled(DTCPolicyLevel level) {}
    virtual void OnInlinePolicyDisabled(DTCPolicyLevel level) {}

   protected:
    virtual ~PolicyObserver() {}
  };

  explicit DeviceTrustConnectorService(PrefService* profile_prefs);

  DeviceTrustConnectorService(const DeviceTrustConnectorService&) = delete;
  DeviceTrustConnectorService& operator=(const DeviceTrustConnectorService&) =
      delete;

  ~DeviceTrustConnectorService() override;

  // Returns whether the Device Trust connector is enabled or not.
  virtual bool IsConnectorEnabled() const;

  // Returns the policy levels at which the current `url` navigation is being
  // watched for.
  const std::set<DTCPolicyLevel> Watches(const GURL& url) const;

  // Adds `observer` to the list of owned policy observers.
  void AddObserver(std::unique_ptr<PolicyObserver> observer);

  // Returns the policy levels the service is enabled for.
  const std::set<DTCPolicyLevel> GetEnabledInlinePolicyLevels() const;

 private:
  // Contains details relating to a policy.
  struct DTCPolicyDetails {
    explicit DTCPolicyDetails(const std::string& pref);
    DTCPolicyDetails(DTCPolicyDetails&& other);
    DTCPolicyDetails& operator=(DTCPolicyDetails&&);
    ~DTCPolicyDetails();

    // Whether the policy is enabled or not.
    bool enabled;

    // The name of the pref that contains the DTC policy values.
    std::string pref;

    // The URL matcher for the current policy.
    std::unique_ptr<url_matcher::URLMatcher> matcher;
  };

  // Called when the DTC policies values change in Prefs for the
  // UserContextAwareAccessSignalsAllowlist policy and the
  // BrowserContextAwareAccessSignalsAllowlist policy. The `pref` is the pref
  // value that changed, and `level` is the policy level of the policy that was
  // updated.
  void OnPolicyUpdated(const DTCPolicyLevel& level, const std::string& pref);

  // Functions used to propagate an update to all observers.
  void OnInlinePolicyEnabled(DTCPolicyLevel level);
  void OnInlinePolicyDisabled(DTCPolicyLevel level);

  // Gets a list of URLs from the specified `pref`.
  const base::Value::List* GetPolicyUrlPatterns(const std::string& pref) const;

  PrefChangeRegistrar pref_observer_;

  raw_ptr<PrefService> profile_prefs_;

  // Maps the policy details per policy level.
  base::flat_map<DTCPolicyLevel, DTCPolicyDetails> policy_details_map_;

  // The URL matcher created from the ContextAwareAccessSignalsAllowlist policy.
  std::unique_ptr<url_matcher::URLMatcher> matcher_;

  // Observers to notify whenever policy values change.
  std::vector<std::unique_ptr<PolicyObserver>> observers_;

  base::WeakPtrFactory<DeviceTrustConnectorService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_H_

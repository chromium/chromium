// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class GURL;

namespace enterprise_connectors {

struct AttestationResponse;
class AttestationService;
class DeviceTrustConnectorService;
struct DeviceTrustResponse;
class SignalsService;

// Main service used to drive device trust connector scenarios. It is currently
// used to generate a response for a crypto challenge received from Verified
// Access during an attestation flow.
class DeviceTrustService : public KeyedService {
 public:
  using DeviceTrustCallback =
      base::OnceCallback<void(const DeviceTrustResponse&)>;

  // Callback used by the data_decoder to get the parsed json result.
  using ParseJsonChallengeCallback =
      base::OnceCallback<void(const std::string&)>;

  DeviceTrustService(std::unique_ptr<AttestationService> attestation_service,
                     std::unique_ptr<SignalsService> signals_service,
                     DeviceTrustConnectorService* connector);

  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

  ~DeviceTrustService() override;

  // Check if DeviceTrustService is enabled.  This method may be called from
  // any task sequence.
  virtual bool IsEnabled() const;

  // Uses the challenge stored in `serialized_challenge` to generate a
  // challenge-response containing device signals and a device identity. This
  // detail, along with the policy `levels` the connector is enabled for, will
  // be used in the  attestation flow to build the challenge response respective
  // to its policy level. Returns the challenge response asynchronously via
  // `callback`.
  virtual void BuildChallengeResponse(const std::string& serialized_challenge,
                                      const std::set<DTCPolicyLevel>& levels,
                                      DeviceTrustCallback callback);

  // Returns the policy levels at which the current `url` navigation is being
  // watched for.
  virtual const std::set<DTCPolicyLevel> Watches(const GURL& url) const;

  // Collects device trust signals and returns them via `callback`.
  void GetSignals(base::OnceCallback<void(base::Value::Dict)> callback);

  // Parses the `serialized_challenge` and returns its value via `callback`.
  void ParseJsonChallenge(const std::string& serialized_challenge,
                          ParseJsonChallengeCallback callback);

 protected:
  // Default constructor that can be used by mocks to bypass initialization.
  DeviceTrustService();

 private:
  void OnChallengeParsed(const std::set<DTCPolicyLevel>& levels,
                         DeviceTrustCallback callback,
                         const std::string& challenge);
  void OnSignalsCollected(const std::string& challenge,
                          const std::set<DTCPolicyLevel>& levels,
                          DeviceTrustCallback callback,
                          base::Value::Dict signals);
  void OnAttestationResponseReceived(
      DeviceTrustCallback callback,
      const AttestationResponse& attestation_response);

  std::unique_ptr<AttestationService> attestation_service_;
  std::unique_ptr<SignalsService> signals_service_;
  const raw_ptr<DeviceTrustConnectorService> connector_{nullptr};
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

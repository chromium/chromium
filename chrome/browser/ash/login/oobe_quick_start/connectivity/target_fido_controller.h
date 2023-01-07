// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_

#include <vector>

#include "base/callback.h"
#include "components/cbor/values.h"
#include "url/origin.h"

namespace ash::quick_start {

class NearbyConnectionsManager;
class TargetFidoControllerTest;

// TargetFidoController initializes the FidoDeviceAuthenticator and the
// GetAssertionRequestHandler to begin the FIDO CTAP2 Assertion Flow. This class
// is also responsible for preparing the GetAssertionRequest and dispatching the
// request.
class TargetFidoController {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  explicit TargetFidoController(
      NearbyConnectionsManager* nearby_connections_manager);
  TargetFidoController(const TargetFidoController&) = delete;
  TargetFidoController& operator=(const TargetFidoController&) = delete;
  ~TargetFidoController();

  void RequestAssertion(const std::string& challenge_b64url,
                        ResultCallback callback);

 private:
  friend class TargetFidoControllerTest;

  cbor::Value GenerateGetAssertionRequest(const std::string& challenge_b64url);

  std::vector<uint8_t> CBOREncodeGetAssertionRequest(
      const cbor::Value& request);

  std::string CreateClientDataJson(const url::Origin& orgin,
                                   const std::string& challenge_b64url);

  // TODO(b/234655072): Remove maybe_unused tag after NearbyConnectionsManager
  // defined.
  [[maybe_unused]] const base::raw_ptr<NearbyConnectionsManager>
      nearby_connections_manager_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_FIDO_CONTROLLER_H_

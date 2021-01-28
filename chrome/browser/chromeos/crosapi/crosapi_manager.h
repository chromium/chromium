// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace crosapi {
namespace mojom {
class BrowserService;
class Crosapi;
}  // namespace mojom

class CrosapiAsh;
class EnvironmentProvider;

// Maintains the crosapi connection provided by ash-chrome.
class CrosapiManager {
 public:
  // Returns the instance of CrosapiManager. It is effectively a singleton.
  static CrosapiManager* Get();

  CrosapiManager();
  CrosapiManager(const CrosapiManager&) = delete;
  CrosapiManager& operator=(const CrosapiManager&) = delete;
  ~CrosapiManager();

  // Binds local_endpoint to BrowserService, and invites to the Mojo universe.
  // Then, request Crosapi pending receiver to the client, and on its callback,
  // binds it to |crosapi_|.
  // Also, BrowserService's version is queried, and on its completion,
  // |completion_callback| is called. |disconnect_handler| invocation is bound
  // to BrowserService at first, but on Crosapi binding, it is transferred to
  // Crosapi. So, before Crosapi binding, |disconnect_handler| is called on
  // BrowserService disconnection. After Crosapi binding, it is called on
  // Crosapi disconnection, but not on BrowserService disconnection.
  void SendInvitation(
      EnvironmentProvider* environment_provider,
      mojo::PlatformChannelEndpoint local_endpoint,
      base::OnceClosure disconnect_handler,
      base::OnceCallback<void(mojo::Remote<mojom::BrowserService>)>
          completion_callback);

 private:
  class InvitationFlow;

  std::unique_ptr<CrosapiAsh> crosapi_;
  std::vector<std::unique_ptr<InvitationFlow>> pending_invitation_flow_list_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_

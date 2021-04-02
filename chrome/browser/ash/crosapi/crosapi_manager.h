// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace crosapi {
class CrosapiAsh;

// Maintains the crosapi connection provided by ash-chrome.
class CrosapiManager {
 public:
  // Returns true if the global CrosapiManager is initialized.
  static bool IsInitialized();

  // Returns the instance of CrosapiManager. It is effectively a singleton.
  static CrosapiManager* Get();

  CrosapiManager();
  CrosapiManager(const CrosapiManager&) = delete;
  CrosapiManager& operator=(const CrosapiManager&) = delete;
  ~CrosapiManager();

  CrosapiAsh* crosapi_ash() { return crosapi_ash_.get(); }

  // Binds |local_endpoint| to Crosapi, and invites the remote to the Mojo
  // universe.
  // Returns CrosapiId corresponding to the bound interface, which can be
  // used for client to know where some sub-interfaces come from.
  // |disconnect_handler| will be called on the Crosapi disconnection.
  CrosapiId SendInvitation(mojo::PlatformChannelEndpoint local_endpoint,
                           base::OnceClosure disconnect_handler);

  // DEPRECATED: TODO(crbug.com/1180712): Remove this after lacros-chrome
  // supporting new command line flag is distributed. Binds local_endpoint to
  // BrowserService, and invites to the Mojo universe. Then, request Crosapi
  // pending receiver to the client, and on its callback, binds it to
  // |crosapi_|. |disconnect_handler| invocation is bound to BrowserService at
  // first, but on Crosapi binding, it is transferred to Crosapi. So, before
  // Crosapi binding, |disconnect_handler| is called on BrowserService
  // disconnection. After Crosapi binding, it is called on Crosapi
  // disconnection, but not on BrowserService disconnection. Returns an
  // identifier representing the crosapi connection. It can be used for client
  // to know where some sub-surfaces come from.
  CrosapiId SendLegacyInvitation(mojo::PlatformChannelEndpoint local_endpoint,
                                 base::OnceClosure disconnect_handler);

 private:
  class LegacyInvitationFlow;

  CrosapiId::Generator crosapi_id_generator_;
  std::unique_ptr<CrosapiAsh> crosapi_ash_;
  std::vector<std::unique_ptr<LegacyInvitationFlow>>
      pending_invitation_flow_list_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_

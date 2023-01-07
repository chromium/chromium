// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"
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
  // Provides an interface for tests to replace real implementations with test
  // implementations.
  explicit CrosapiManager(CrosapiDependencyRegistry* registry);
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
 private:
  // Default dependency registry which provides creates prod impls.
  CrosapiDependencyRegistry default_registry_;
  CrosapiId::Generator crosapi_id_generator_;
  std::unique_ptr<CrosapiAsh> crosapi_ash_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_MANAGER_H_

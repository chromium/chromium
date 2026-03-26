// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "url/origin.h"

namespace password_manager {
class PasswordStoreInterface;
}

namespace actor_login {

class ActorLoginDuplicatePermissionCleaner;
class ActorLoginPermissionService;

// Service responsible for clearing permissions. It creates and manages
// one-off `ActorLoginDuplicatePermissionCleaner` instances for each request.
class ActorLoginPermissionCleaningService : public KeyedService {
 public:
  ActorLoginPermissionCleaningService(
      ActorLoginPermissionService* permission_service,
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store);

  ActorLoginPermissionCleaningService(
      const ActorLoginPermissionCleaningService&) = delete;
  ActorLoginPermissionCleaningService& operator=(
      const ActorLoginPermissionCleaningService&) = delete;

  ~ActorLoginPermissionCleaningService() override;

  // KeyedService:
  void Shutdown() override;

  // Starts the asynchronous process of fetching and clearing duplicate
  // permissions.
  // `credential` and `signon_realm` are used to identify which
  // permission to skip, so as to not remove the newly granted permission.
  // `signon_realm` is only needed if the new permission was granted
  // to a password credential.
  void ClearPermissions(const Credential& credential,
                        std::optional<std::string> signon_realm,
                        base::OnceClosure done_callback);

 private:
  // Called by a cleaner when it finishes its asynchronous work.
  void OnCleanerDone(ActorLoginDuplicatePermissionCleaner* cleaner);

  raw_ptr<ActorLoginPermissionService> permission_service_ = nullptr;
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store_;
  scoped_refptr<password_manager::PasswordStoreInterface> account_store_;

  std::set<std::unique_ptr<ActorLoginDuplicatePermissionCleaner>,
           base::UniquePtrComparator>
      active_cleaners_;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_

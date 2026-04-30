// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_permission_cleaning_service.h"
#include "url/origin.h"

namespace password_manager {
class PasswordStoreInterface;
}

namespace actor_login {

class ActorLoginDuplicatePermissionCleaner;
class ActorLoginPermissionService;

class ActorLoginPermissionCleaningServiceImpl
    : public ActorLoginPermissionCleaningService {
 public:
  ActorLoginPermissionCleaningServiceImpl(
      ActorLoginPermissionService* permission_service,
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store);

  ActorLoginPermissionCleaningServiceImpl(
      const ActorLoginPermissionCleaningServiceImpl&) = delete;
  ActorLoginPermissionCleaningServiceImpl& operator=(
      const ActorLoginPermissionCleaningServiceImpl&) = delete;

  ~ActorLoginPermissionCleaningServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // ActorLoginPermissionCleaningService:
  void ClearConflictingPermissions(const Credential& credential,
                                   base::OnceClosure done_callback) override;

 private:
  void OnCleanerDone(ActorLoginDuplicatePermissionCleaner* cleaner);

  raw_ptr<ActorLoginPermissionService> permission_service_ = nullptr;
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store_;
  scoped_refptr<password_manager::PasswordStoreInterface> account_store_;

  std::set<std::unique_ptr<ActorLoginDuplicatePermissionCleaner>,
           base::UniquePtrComparator>
      active_cleaners_;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_IMPL_H_

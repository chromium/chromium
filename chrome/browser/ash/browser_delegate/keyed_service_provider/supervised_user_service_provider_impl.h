// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SUPERVISED_USER_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SUPERVISED_USER_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/supervised_user/supervised_user_service_provider.h"

namespace ash {

class SupervisedUserServiceProviderImpl : public SupervisedUserServiceProvider {
 public:
  SupervisedUserServiceProviderImpl();
  SupervisedUserServiceProviderImpl(const SupervisedUserServiceProviderImpl&) =
      delete;
  SupervisedUserServiceProviderImpl& operator=(
      const SupervisedUserServiceProviderImpl&) = delete;
  ~SupervisedUserServiceProviderImpl() override;

  // SupervisedUserServiceProvider:
  supervised_user::SupervisedUserService* Find(
      const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SUPERVISED_USER_SERVICE_PROVIDER_IMPL_H_

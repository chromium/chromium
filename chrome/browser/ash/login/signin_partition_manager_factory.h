// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash::login {

class SigninPartitionManager;

class SigninPartitionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static SigninPartitionManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SigninPartitionManagerFactory* GetInstance();

  SigninPartitionManagerFactory(const SigninPartitionManagerFactory&) = delete;
  SigninPartitionManagerFactory& operator=(
      const SigninPartitionManagerFactory&) = delete;

 private:
  friend base::NoDestructor<SigninPartitionManagerFactory>;

  SigninPartitionManagerFactory();
  ~SigninPartitionManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash::login

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_FACTORY_H_

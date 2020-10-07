// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace borealis {

class BorealisContextManager;

class BorealisContextManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BorealisContextManager* GetForProfile(Profile* profile);
  static BorealisContextManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<BorealisContextManagerFactory>;

  BorealisContextManagerFactory();
  BorealisContextManagerFactory(const BorealisContextManagerFactory&) = delete;
  BorealisContextManagerFactory& operator=(
      const BorealisContextManagerFactory&) = delete;
  ~BorealisContextManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_FACTORY_H_

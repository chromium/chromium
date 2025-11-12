// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class BocaReceiverService;

// Singleton that owns and builds BocaReceiverService.
class BocaReceiverServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BocaReceiverServiceFactory* GetInstance();
  static BocaReceiverService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<BocaReceiverServiceFactory>;

  BocaReceiverServiceFactory();
  ~BocaReceiverServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_FACTORY_H_

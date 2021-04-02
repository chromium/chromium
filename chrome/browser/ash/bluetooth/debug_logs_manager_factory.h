// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace ash {

namespace bluetooth {

class DebugLogsManager;

// Factory for DebugLogsManager.
class DebugLogsManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DebugLogsManager* GetForProfile(Profile* profile);
  static DebugLogsManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DebugLogsManagerFactory>;

  DebugLogsManagerFactory();
  ~DebugLogsManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(DebugLogsManagerFactory);
};

}  // namespace bluetooth

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
namespace bluetooth {
using ::ash::bluetooth::DebugLogsManagerFactory;
}  // namespace bluetooth
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_

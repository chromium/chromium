// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

namespace bluetooth {

class DebugLogsManager;

// Factory for DebugLogsManager.
class DebugLogsManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static DebugLogsManager* GetForProfile(Profile* profile);
  static DebugLogsManagerFactory* GetInstance();

  DebugLogsManagerFactory(const DebugLogsManagerFactory&) = delete;
  DebugLogsManagerFactory& operator=(const DebugLogsManagerFactory&) = delete;

 private:
  friend base::NoDestructor<DebugLogsManagerFactory>;

  DebugLogsManagerFactory();
  ~DebugLogsManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace bluetooth

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BLUETOOTH_DEBUG_LOGS_MANAGER_FACTORY_H_

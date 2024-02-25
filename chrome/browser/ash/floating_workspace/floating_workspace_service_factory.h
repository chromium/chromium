// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
class FloatingWorkspaceService;
}

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// Singleton factory that builds and owns FloatingWorkspaceService.
class FloatingWorkspaceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FloatingWorkspaceServiceFactory* GetInstance();

  static FloatingWorkspaceService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<FloatingWorkspaceServiceFactory>;

  FloatingWorkspaceServiceFactory();
  ~FloatingWorkspaceServiceFactory() override;

  FloatingWorkspaceServiceFactory(const FloatingWorkspaceServiceFactory&) =
      delete;
  FloatingWorkspaceServiceFactory& operator=(
      const FloatingWorkspaceServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_FACTORY_H_

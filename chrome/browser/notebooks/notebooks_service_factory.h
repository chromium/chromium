// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace notebooks {
class NotebooksService;

class NotebooksServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static NotebooksService* GetForProfile(Profile* profile);

  static NotebooksServiceFactory* GetInstance();

  NotebooksServiceFactory(const NotebooksServiceFactory&) = delete;
  NotebooksServiceFactory& operator=(const NotebooksServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NotebooksServiceFactory>;

  NotebooksServiceFactory();
  ~NotebooksServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace notebooks

#endif  // CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_SERVICE_FACTORY_H_

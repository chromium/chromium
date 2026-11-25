// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_FACTORY_H_
#define CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace critical_actions {

class CriticalActionService;

// Name of the critical actions database file in the profile directory.
inline constexpr base::FilePath::CharType kCriticalActionsDatabaseFilename[] =
    FILE_PATH_LITERAL("CriticalActions.db");

// A factory that creates and manages `CriticalActionService` instances
// for each eligible `Profile`.
class CriticalActionFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of `CriticalActionFactory`.
  static CriticalActionFactory* GetInstance();

  // Retrieves the `CriticalActionService` instance associated with the given
  // `profile`, creating one if it does not exist yet. Returns nullptr if
  // `profile` is off-the-record (incognito) or otherwise ineligible.
  static CriticalActionService* GetForProfile(Profile* profile);

  // Like `GetForProfile()`, but returns nullptr instead of creating the
  // service if it does not exist yet. Used by tests to verify that the service
  // is created together with the profile (see
  // `ServiceIsCreatedWithBrowserContext()`), which `GetForProfile()` cannot
  // check because it creates the service on demand.
  static CriticalActionService* GetForProfileIfExistsForTesting(
      Profile* profile);

  CriticalActionFactory(const CriticalActionFactory&) = delete;
  CriticalActionFactory& operator=(const CriticalActionFactory&) = delete;

 private:
  friend base::NoDestructor<CriticalActionFactory>;

  CriticalActionFactory();
  ~CriticalActionFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_CRITICAL_ACTIONS_CRITICAL_ACTION_FACTORY_H_

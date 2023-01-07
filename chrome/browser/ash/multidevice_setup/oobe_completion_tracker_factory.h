// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace ash {
namespace multidevice_setup {

class OobeCompletionTracker;

// Owns OobeCompletionTracker instances and associates them with Profiles.
class OobeCompletionTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static OobeCompletionTracker* GetForProfile(Profile* profile);

  static OobeCompletionTrackerFactory* GetInstance();

  OobeCompletionTrackerFactory(const OobeCompletionTrackerFactory&) = delete;
  OobeCompletionTrackerFactory& operator=(const OobeCompletionTrackerFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<OobeCompletionTrackerFactory>;

  OobeCompletionTrackerFactory();
  ~OobeCompletionTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace multidevice_setup
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_

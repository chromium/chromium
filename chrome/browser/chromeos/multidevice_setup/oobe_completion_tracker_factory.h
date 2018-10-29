// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace chromeos {

namespace multidevice_setup {

class OobeCompletionTracker;

// Owns OobeCompletionTracker instances and associates them with Profiles.
class OobeCompletionTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OobeCompletionTracker* GetForProfile(Profile* profile);

  static OobeCompletionTrackerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<OobeCompletionTrackerFactory>;

  OobeCompletionTrackerFactory();
  ~OobeCompletionTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OobeCompletionTrackerFactory);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_OOBE_COMPLETION_TRACKER_FACTORY_H_

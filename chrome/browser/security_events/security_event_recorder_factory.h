// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename t>
struct DefaultSingletonTraits;
}

class Profile;
class SecurityEventRecorder;

class SecurityEventRecorderFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of SecurityEventRecorderFactory.
  static SecurityEventRecorderFactory* GetInstance();

  // Returns the SecurityEventRecorder associated with |profile|.
  static SecurityEventRecorder* GetForProfile(Profile* profile);

  SecurityEventRecorderFactory(const SecurityEventRecorderFactory&) = delete;
  SecurityEventRecorderFactory& operator=(const SecurityEventRecorderFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<SecurityEventRecorderFactory>;

  SecurityEventRecorderFactory();
  ~SecurityEventRecorderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};
#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_

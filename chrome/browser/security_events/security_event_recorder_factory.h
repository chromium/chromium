// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename t>
struct DefaultSingletonTraits;
}

class Profile;
class SecurityEventRecorder;

class SecurityEventRecorderFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton instance of SecurityEventRecorderFactory.
  static SecurityEventRecorderFactory* GetInstance();

  // Returns the SecurityEventRecorder associated with |profile|.
  static SecurityEventRecorder* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SecurityEventRecorderFactory>;

  SecurityEventRecorderFactory();
  ~SecurityEventRecorderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SecurityEventRecorderFactory);
};
#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_FACTORY_H_

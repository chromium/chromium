// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class AccessibilityLabelsService;

// Factory to get or create an instance of AccessibilityLabelsService from
// a Profile.
class AccessibilityLabelsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityLabelsService* GetForProfile(Profile* profile);

  static AccessibilityLabelsService* GetForProfileIfExists(Profile* profile);

  static AccessibilityLabelsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<AccessibilityLabelsServiceFactory>;

  AccessibilityLabelsServiceFactory();
  ~AccessibilityLabelsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_FACTORY_H_

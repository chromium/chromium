// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace screen_ai {

class AXMainNodeAnnotatorController;

// Factory to get or create an instance of AXMainNodeAnnotatorController from a
// Profile.
class AXMainNodeAnnotatorControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static AXMainNodeAnnotatorController* GetForProfile(Profile* profile);

  static AXMainNodeAnnotatorController* GetForProfileIfExists(Profile* profile);

  static AXMainNodeAnnotatorControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<AXMainNodeAnnotatorControllerFactory>;

  AXMainNodeAnnotatorControllerFactory();
  ~AXMainNodeAnnotatorControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_FACTORY_H_

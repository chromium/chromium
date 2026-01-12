// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SKILLS_SKILLS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace skills {

class SkillsService;

// Factory for SkillsService.
class SkillsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SkillsService* GetForProfile(Profile* profile);

  static SkillsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SkillsServiceFactory>;

  SkillsServiceFactory();
  ~SkillsServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_SERVICE_FACTORY_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace context_hub {
class SkillRecordingService;
}  // namespace context_hub

// Factory for creating and retrieving per-profile `SkillRecordingService`
// instances.
class SkillRecordingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static context_hub::SkillRecordingService* GetForProfile(Profile* profile);
  static SkillRecordingServiceFactory* GetInstance();

  SkillRecordingServiceFactory(const SkillRecordingServiceFactory&) = delete;
  SkillRecordingServiceFactory& operator=(const SkillRecordingServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SkillRecordingServiceFactory>;

  SkillRecordingServiceFactory();
  ~SkillRecordingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_FACTORY_H_

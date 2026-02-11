// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace record_replay {

class RecordingDataManager;

class RecordingDataManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static RecordingDataManager* GetForProfile(Profile* profile);
  static RecordingDataManagerFactory* GetInstance();

  RecordingDataManagerFactory(const RecordingDataManagerFactory&) = delete;
  RecordingDataManagerFactory& operator=(const RecordingDataManagerFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

 private:
  friend base::NoDestructor<RecordingDataManagerFactory>;

  RecordingDataManagerFactory();
  ~RecordingDataManagerFactory() override;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_FACTORY_H_

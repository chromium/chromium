// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PREFS_WATCHER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PREFS_WATCHER_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos::cloud_upload {

// This factory reacts to profile creation and instantiates profile-keyed
// services that set up watchers for prefs related to Cloud Upload.
class CloudUploadPrefsWatcherFactory : public ProfileKeyedServiceFactory {
 public:
  static CloudUploadPrefsWatcherFactory* GetInstance();

  CloudUploadPrefsWatcherFactory(const CloudUploadPrefsWatcherFactory&) =
      delete;
  CloudUploadPrefsWatcherFactory& operator=(
      const CloudUploadPrefsWatcherFactory&) = delete;

 private:
  friend base::NoDestructor<CloudUploadPrefsWatcherFactory>;

  CloudUploadPrefsWatcherFactory();
  ~CloudUploadPrefsWatcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PREFS_WATCHER_H_

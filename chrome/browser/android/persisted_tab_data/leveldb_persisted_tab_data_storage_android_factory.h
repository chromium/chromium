// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_FACTORY_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class LevelDBPersistedTabDataStorageAndroid;

class LevelDBPersistedTabDataStorageAndroidFactory
    : public ProfileKeyedServiceFactory {
 public:
  LevelDBPersistedTabDataStorageAndroidFactory(
      const LevelDBPersistedTabDataStorageAndroidFactory&) = delete;
  LevelDBPersistedTabDataStorageAndroidFactory& operator=(
      const LevelDBPersistedTabDataStorageAndroidFactory&) = delete;

  static LevelDBPersistedTabDataStorageAndroidFactory* GetInstance();
  static LevelDBPersistedTabDataStorageAndroid* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<LevelDBPersistedTabDataStorageAndroidFactory>;

  LevelDBPersistedTabDataStorageAndroidFactory();
  ~LevelDBPersistedTabDataStorageAndroidFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_FACTORY_H_

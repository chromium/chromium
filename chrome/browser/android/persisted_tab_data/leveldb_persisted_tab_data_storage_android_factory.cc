// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android_factory.h"

#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"

// static
LevelDBPersistedTabDataStorageAndroidFactory*
LevelDBPersistedTabDataStorageAndroidFactory::GetInstance() {
  static base::NoDestructor<LevelDBPersistedTabDataStorageAndroidFactory>
      instance;
  return instance.get();
}

// static
LevelDBPersistedTabDataStorageAndroid*
LevelDBPersistedTabDataStorageAndroidFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord()) {
    return nullptr;
  }
  return static_cast<LevelDBPersistedTabDataStorageAndroid*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

LevelDBPersistedTabDataStorageAndroidFactory::
    LevelDBPersistedTabDataStorageAndroidFactory()
    : ProfileKeyedServiceFactory(
          "LevelDBPersistedTabDataStorageAndroid",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SessionProtoDBFactory<
            persisted_state_db::PersistedStateContentProto>::GetInstance());
}

KeyedService*
LevelDBPersistedTabDataStorageAndroidFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new LevelDBPersistedTabDataStorageAndroid(profile);
}

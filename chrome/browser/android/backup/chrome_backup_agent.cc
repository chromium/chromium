// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/values.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_prefs.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeBackupAgentImpl_jni.h"

static_assert(14 == static_cast<int>(syncer::UserSelectableType::kLastType),
              "When adding a new selectable type, add its pref to"
              "BoolPrefBackupSerializer if the type exists on Android");

void JNI_ChromeBackupAgentImpl_CommitPendingPrefWrites(
    JNIEnv* env,
    PrefService* pref_service) {
  // TODO(crbug.com/332710541): This currently doesn't wait for the commit to
  // complete (it passes the default value for the reply_callback param). Wait
  // for the commit to complete, here or in Java.
  pref_service->CommitPendingWrite();
}

void JNI_ChromeBackupAgentImpl_MigrateGlobalDataTypePrefsToAccount(
    JNIEnv* env,
    PrefService* pref_service,
    std::string& gaia_id) {
  syncer::SyncPrefs sync_prefs(pref_service);
  sync_prefs.MigrateGlobalDataTypePrefsToAccount(
      pref_service, signin::GaiaIdHash::FromGaiaId(gaia_id));
}

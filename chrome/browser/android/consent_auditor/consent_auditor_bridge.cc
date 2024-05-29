// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/identity_manager/account_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/consent_auditor/android/jni_headers/ConsentAuditorBridge_jni.h"

using base::android::JavaParamRef;

static void JNI_ConsentAuditorBridge_RecordConsent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    Profile* profile,
    const JavaParamRef<jobject>& j_account_id,
    jint j_feature,
    const JavaParamRef<jintArray>& j_consent_description,
    jint j_consent_confirmation) {
  // TODO(markusheintz): Update the ConsentAuditorBridgeInterface.
  DCHECK_EQ(static_cast<consent_auditor::Feature>(j_feature),
            consent_auditor::Feature::CHROME_SYNC);

  std::vector<int> consent_description;
  base::android::JavaIntArrayToIntVector(env, j_consent_description,
                                         &consent_description);

  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                              UserConsentTypes_ConsentStatus_GIVEN);
  sync_consent.set_confirmation_grd_id(j_consent_confirmation);
  for (int id : consent_description) {
    sync_consent.add_description_grd_ids(id);
  }
  ConsentAuditorFactory::GetForProfile(profile)->RecordSyncConsent(
      ConvertFromJavaCoreAccountId(env, j_account_id), sync_consent);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/OfflineContentAggregatorFactory_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/offline_items_collection/core/android/offline_content_aggregator_bridge.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"

using base::android::JavaParamRef;

// Takes a Java Profile and returns a Java OfflineContentAggregatorBridge.
static base::android::ScopedJavaLocalRef<jobject>
JNI_OfflineContentAggregatorFactory_GetOfflineContentAggregator(JNIEnv* env) {
  ProfileKey* profile_key = ::android::GetLastUsedProfileKey();
  DCHECK(profile_key);
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetInstance()->GetForKey(profile_key);
  return offline_items_collection::android::OfflineContentAggregatorBridge::
      GetBridgeForOfflineContentAggregator(aggregator);
}

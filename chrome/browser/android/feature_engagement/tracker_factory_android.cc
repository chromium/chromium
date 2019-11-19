// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/TrackerFactory_jni.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feature_engagement/public/tracker.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_TrackerFactory_GetTrackerForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  return feature_engagement::Tracker::GetJavaObject(
      feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
          profile));
}

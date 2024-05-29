// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/android/wrapping_test_tracker.h"
#include "components/feature_engagement/public/tracker.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feature_engagement/jni_headers/TrackerFactory_jni.h"

namespace {

std::unique_ptr<KeyedService> CreateWrapperTrackerFactory(
    base::android::ScopedJavaGlobalRef<jobject> jtracker,
    content::BrowserContext* context) {
  return base::WrapUnique(
      new feature_engagement::WrappingTestTracker(jtracker));
}

}  // namespace

static base::android::ScopedJavaLocalRef<jobject>
JNI_TrackerFactory_GetTrackerForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);
  return feature_engagement::Tracker::GetJavaObject(
      feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
          profile));
}

static void JNI_TrackerFactory_SetTestingFactory(
    JNIEnv* env,
    Profile* profile,
    const base::android::JavaParamRef<jobject>& jtracker) {
  DCHECK(profile);

  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(
                   &CreateWrapperTrackerFactory,
                   base::android::ScopedJavaGlobalRef<jobject>(jtracker)));
}

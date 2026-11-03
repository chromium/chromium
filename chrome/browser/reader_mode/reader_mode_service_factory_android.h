// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_ANDROID_H_
#define CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_ANDROID_H_

#include "base/android/scoped_java_ref.h"

class Profile;

namespace reader_mode::android {

// This class should not be used except from the Java class
// ReaderModeServiceFactory.
class ReaderModeServiceFactoryAndroid {
 public:
  static base::android::ScopedJavaLocalRef<jobject> GetForProfile(
      JNIEnv* env,
      Profile* profile);
};

}  // namespace reader_mode::android

#endif  // CHROME_BROWSER_READER_MODE_READER_MODE_SERVICE_FACTORY_ANDROID_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_BRIDGE_H_
#define CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"

using base::android::ScopedJavaLocalRef;

namespace data_sharing {

class DataSharingServiceFactoryBridge {
 public:
  DataSharingServiceFactoryBridge();
  ~DataSharingServiceFactoryBridge();

  static ScopedJavaLocalRef<jobject> CreateJavaSDKDelegate(Profile* profile);
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DATA_SHARING_SERVICE_FACTORY_BRIDGE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_ANDROID_FINDS_SERVICE_ANDROID_H_
#define CHROME_BROWSER_FINDS_ANDROID_FINDS_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/finds/core/finds_service.h"

namespace finds {

// Android bridge for FindsService. This class observes the C++ FindsService and
// forwards notifications to its Java counterpart.
class FindsServiceAndroid : public FindsService::Observer,
                            public base::SupportsUserData::Data {
 public:
  explicit FindsServiceAndroid(FindsService* service);
  ~FindsServiceAndroid() override;

  FindsServiceAndroid(const FindsServiceAndroid&) = delete;
  FindsServiceAndroid& operator=(const FindsServiceAndroid&) = delete;

  // FindsService::Observer implementation:
  void OnOptInCriteriaFulfilled() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  raw_ptr<FindsService> service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_ANDROID_FINDS_SERVICE_ANDROID_H_

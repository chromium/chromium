// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_ANDROID_DATA_SHARING_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_DATA_SHARING_ANDROID_DATA_SHARING_UI_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {

// Android side implementation of a DataSharingUIDelegate.
class DataSharingUIDelegateAndroid : public DataSharingUIDelegate {
 public:
  explicit DataSharingUIDelegateAndroid(Profile* profile);
  ~DataSharingUIDelegateAndroid() override;

  // DataSharingUIDelegate implementation.
  void HandleShareURLIntercepted(
      const GURL& url,
      std::unique_ptr<ShareURLInterceptionContext> context) override;

  ScopedJavaLocalRef<jobject> GetJavaObject() override;

 private:
  void LazyInitializeIfNeeded();

  ScopedJavaGlobalRef<jobject> java_obj_;

  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_ANDROID_DATA_SHARING_UI_DELEGATE_ANDROID_H_

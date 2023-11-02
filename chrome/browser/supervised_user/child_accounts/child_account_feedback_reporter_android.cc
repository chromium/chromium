// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ChildAccountFeedbackReporter_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;

void ReportChildAccountFeedback(content::WebContents* web_contents,
                                const std::string& description,
                                const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ui::WindowAndroid* window = web_contents->GetNativeView()->GetWindowAndroid();

  ScopedJavaLocalRef<jstring> jdesc =
      base::android::ConvertUTF8ToJavaString(env, description);
  ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  ScopedJavaLocalRef<jobject> jprofile = profile_android->GetJavaObject();
  Java_ChildAccountFeedbackReporter_reportFeedbackWithWindow(
      env, window->GetJavaObject(), jdesc, jurl, jprofile);
}

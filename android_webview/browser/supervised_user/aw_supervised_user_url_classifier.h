// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "url/gurl.h"

namespace android_webview {
using UrlClassifierCallback = base::OnceCallback<void(bool /*shouldBlock*/)>;

// Native side of java-class of same name.
//
// Lifetime: Singleton
class AwSupervisedUserUrlClassifier {
 public:
  static AwSupervisedUserUrlClassifier* GetInstance();

  AwSupervisedUserUrlClassifier(const AwSupervisedUserUrlClassifier&) = delete;
  AwSupervisedUserUrlClassifier& operator=(
      const AwSupervisedUserUrlClassifier&) = delete;

  bool ShouldCreateThrottle();

  void ShouldBlockUrl(const GURL& request_url, UrlClassifierCallback callback);

 private:
  AwSupervisedUserUrlClassifier();
  ~AwSupervisedUserUrlClassifier() = default;

  bool shouldCreateThrottle_ = false;
  friend class base::NoDestructor<AwSupervisedUserUrlClassifier>;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_

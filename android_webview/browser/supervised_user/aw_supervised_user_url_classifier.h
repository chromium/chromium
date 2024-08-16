// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace android_webview {

namespace prefs {
inline constexpr char kShouldBlockRestrictedContent[] =
    "android_webview.should_block_restricted_content";
}  // namespace prefs

using UrlClassifierCallback = base::OnceCallback<void(bool /*shouldBlock*/)>;

// Native side of java-class of same name. Must only be used on the UI thread.
//
// Lifetime: Singleton
class AwSupervisedUserUrlClassifier {
 public:
  static AwSupervisedUserUrlClassifier* GetInstance();
  static void RegisterPrefs(PrefRegistrySimple* registry);

  AwSupervisedUserUrlClassifier(const AwSupervisedUserUrlClassifier&) = delete;
  AwSupervisedUserUrlClassifier& operator=(
      const AwSupervisedUserUrlClassifier&) = delete;

  bool ShouldCreateThrottle();

  void ShouldBlockUrl(const GURL& request_url, UrlClassifierCallback callback);

  void SetUserRequiresUrlChecks(bool user_requires_url_checks);

 private:
  AwSupervisedUserUrlClassifier();
  ~AwSupervisedUserUrlClassifier() = default;

  bool platform_supports_url_checks_ = false;
  raw_ptr<PrefService> local_state_;
  friend class base::NoDestructor<AwSupervisedUserUrlClassifier>;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_URL_CLASSIFIER_H_

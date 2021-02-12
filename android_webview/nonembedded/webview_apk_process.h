// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_WEBVIEW_APK_PROCESS_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_WEBVIEW_APK_PROCESS_H_

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace android_webview {
// Class that holds global state in the webview apk process.
class WebViewApkProcess {
 public:
  static WebViewApkProcess* GetInstance();
  PrefService* GetPrefService();

 private:
  friend class base::NoDestructor<WebViewApkProcess>;

  WebViewApkProcess();
  ~WebViewApkProcess();
  void CreatePrefService();
  void RegisterPrefs(PrefRegistrySimple* pref_registry);

  std::unique_ptr<PrefService> pref_service_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_WEBVIEW_APK_PROCESS_H_

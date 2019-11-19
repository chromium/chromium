// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

// The C++ counterpart to LogoBridge.java. Enables Java code to access the
// default search provider's logo.
class LogoBridge {
 public:
  explicit LogoBridge(const base::android::JavaRef<jobject>& j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // TODO(treib): Double-check the observer contract (esp. for
  // LogoService::GetLogo()).
  //
  // Gets the current non-animated logo (downloading it if necessary) and passes
  // it to the observer.
  // The observer's |onLogoAvailable| is guaranteed to be called at least once:
  // a) A cached doodle is available.
  // b) A new doodle is available.
  // c) Not having a doodle was revalidated.
  void GetCurrentLogo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_logo_observer);

 private:
  virtual ~LogoBridge();

  search_provider_logos::LogoService* logo_service_;

  base::WeakPtrFactory<LogoBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LogoBridge);
};

#endif  // CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"

namespace content {
class WebContents;
}

class SkBitmap;
struct ShortcutInfo;
struct AddToHomescreenParams;

// AddToHomescreenManager is the C++ counterpart of
// org.chromium.chrome.browser's AddToHomescreenManager in Java. This object
// fetches the data required to add a URL to home screen. It is owned by the
// Java counterpart, created from there via a JNI Start() call and MUST BE
// DESTROYED via Destroy().
class AddToHomescreenManager : public AddToHomescreenDataFetcher::Observer {
 public:
  AddToHomescreenManager(JNIEnv* env, jobject obj);

  // Called by the Java counterpart to destroy this object.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Adds the current URL to the Android home screen using |title|. Depending on
  // the site, the shortcut may be in the form of a bookmark shortcut or a
  // WebAPK (which ignores the provided title).
  void AddToHomescreen(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jstring>& title);

  // Starts the process of fetching data required for add to home screen.
  void Start(content::WebContents* web_contents);

 private:
  ~AddToHomescreenManager() override;

  void RecordEventForAppMenu(AddToHomescreenInstaller::Event event,
                             const AddToHomescreenParams& a2hs_params);

  // AddToHomescreenDataFetcher::Observer:
  void OnUserTitleAvailable(const base::string16& user_title,
                            const GURL& url,
                            bool is_webapk_compatible) override;
  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       const SkBitmap& badge_icon) override;

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Whether the site is WebAPK-compatible.
  bool is_webapk_compatible_;

  // Fetches data required to add a shortcut.
  std::unique_ptr<AddToHomescreenDataFetcher> data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenManager);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_

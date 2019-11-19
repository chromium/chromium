// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_UI_DELEGATE_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"
#include "chrome/browser/android/webapps/add_to_homescreen_params.h"

namespace banners {

class AppBannerManager;

// Delegate provided to the app banner UI surfaces to install a web app or
// native app.
class AppBannerUiDelegateAndroid {
 public:
  // Creates a delegate for promoting the installation of a web app.
  static std::unique_ptr<AppBannerUiDelegateAndroid> Create(
      base::WeakPtr<AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  ~AppBannerUiDelegateAndroid();

  // Called through the JNI to add the app described by this class to home
  // screen.
  void AddToHomescreen(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // Called through the JNI to indicate that the user has dismissed the
  // installation UI.
  void OnUiCancelled(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Called by the UI layer to indicate that the user has dismissed the
  // installation UI.
  void OnUiCancelled();

  // Called to show a modal app banner. Returns true if the dialog is
  // successfully shown.
  bool ShowDialog();

  // Called by the UI layer to display the details for a native app.
  bool ShowNativeAppDetails();

  bool ShowNativeAppDetails(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

 private:
  // Delegate for promoting a web or Android app.
  AppBannerUiDelegateAndroid(
      base::WeakPtr<AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  void CreateJavaDelegate();

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  base::WeakPtr<AppBannerManager> weak_manager_;

  std::unique_ptr<AddToHomescreenParams> params_;

  const base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                     const AddToHomescreenParams&)>
      event_callback_;
  DISALLOW_COPY_AND_ASSIGN(AppBannerUiDelegateAndroid);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_UI_DELEGATE_ANDROID_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace media_router {

// Android implementation of the MediaRouterDialogController.
class MediaRouterDialogControllerAndroid
    : public content::WebContentsUserData<MediaRouterDialogControllerAndroid>,
      public MediaRouterDialogController {
 public:
  ~MediaRouterDialogControllerAndroid() override;

  // The methods called by the Java counterpart.

  // Notifies the controller that user has selected a sink with |jsink_id| for
  // |source_id|.
  void OnSinkSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& source_id,
                      const base::android::JavaParamRef<jstring>& jsink_id);
  // Notifies the controller that user chose to close the route.
  void OnRouteClosed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jmedia_route_id);
  // Notifies the controller that the dialog has been closed without the user
  // taking any action (e.g. closing the route or selecting a sink).
  void OnDialogCancelled(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  // Notifies the controller the media source URN is not supported so it could
  // properly reject the request.
  void OnMediaSourceNotSupported(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  friend class content::WebContentsUserData<MediaRouterDialogControllerAndroid>;

  // Use MediaRouterDialogControllerAndroid::CreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogControllerAndroid(
      content::WebContents* web_contents);

  // MediaRouterDialogController:
  void CreateMediaRouterDialog() override;
  void CloseMediaRouterDialog() override;
  bool IsShowingMediaRouterDialog() const override;

  void CancelPresentationRequest();

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerAndroid);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_ANDROID_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/DocumentPictureInPictureActivity_jni.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

using content::WebContents;

class DocumentPictureInPictureCCTActivity {
 public:
  void NonStatic(JNIEnv* env);
};

static void JNI_DocumentPictureInPictureActivity_OnActivityStart(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& parentWebContent,
    const jni_zero::JavaParamRef<jobject>& webContent) {
  WebContents* parent_web_contents =
      WebContents::FromJavaWebContents(parentWebContent);
  WebContents* web_content = WebContents::FromJavaWebContents(webContent);
  PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
      parent_web_contents, web_content);
}

DEFINE_JNI(DocumentPictureInPictureActivity)

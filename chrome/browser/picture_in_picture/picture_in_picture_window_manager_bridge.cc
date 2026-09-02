// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PictureInPictureWindowManagerBridge_jni.h"

namespace picture_in_picture {

class PictureInPictureWindowManagerBridge
    : public PictureInPictureWindowManager::Observer {
 public:
  static PictureInPictureWindowManagerBridge* GetInstance() {
    static base::NoDestructor<PictureInPictureWindowManagerBridge> instance;
    return instance.get();
  }

  PictureInPictureWindowManagerBridge() {
    PictureInPictureWindowManager::GetInstance()->AddObserver(this);
  }

  PictureInPictureWindowManagerBridge(
      const PictureInPictureWindowManagerBridge&) = delete;
  PictureInPictureWindowManagerBridge& operator=(
      const PictureInPictureWindowManagerBridge&) = delete;

  ~PictureInPictureWindowManagerBridge() override = default;

  // PictureInPictureWindowManager::Observer:
  void OnEnterPictureInPicture() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PictureInPictureWindowManagerBridge_onPictureInPictureStateChanged(
        env, true);
  }

  void OnExitPictureInPicture() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PictureInPictureWindowManagerBridge_onPictureInPictureStateChanged(
        env, false);
  }
};

static bool JNI_PictureInPictureWindowManagerBridge_IsInPictureInPicture(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PictureInPictureWindowManagerBridge::GetInstance();
  return PictureInPictureWindowManager::GetInstance()->IsInPictureInPicture();
}

}  // namespace picture_in_picture

DEFINE_JNI(PictureInPictureWindowManagerBridge)

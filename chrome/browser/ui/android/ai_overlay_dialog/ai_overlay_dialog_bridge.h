// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "build/android_buildflags.h"

class BrowserWindowInterface;

namespace ttc {

#if BUILDFLAG(IS_DESKTOP_ANDROID)
class AiOverlayDialogControllerAndroid;
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

// Native counterpart of the Java |AiOverlayDialogBridge|.
//
// Owns the window-scoped |AiOverlayDialogControllerAndroid|. The Java side is a
// ChromeAndroidTaskFeature, so this is destroyed before the
// BrowserWindowInterface the controller attaches itself to.
//
// The Java class, and therefore this JNI, ships in every Android APK; only
// Desktop Android ever instantiates it.
class AiOverlayDialogBridge final {
 public:
  AiOverlayDialogBridge(JNIEnv* env,
                        const base::android::JavaRef<jobject>& java_bridge,
                        BrowserWindowInterface* browser);
  AiOverlayDialogBridge(const AiOverlayDialogBridge&) = delete;
  AiOverlayDialogBridge& operator=(const AiOverlayDialogBridge&) = delete;
  ~AiOverlayDialogBridge();

  // Implements Java |AiOverlayDialogBridge.Natives#destroy|.
  void Destroy(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef java_bridge_;
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  std::unique_ptr<AiOverlayDialogControllerAndroid> controller_;
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
};

// Forwards `energy` to the audio energy listener registered for `browser`'s
// window. No-op if `browser` has no window or no listener is registered.
void UpdateAudioEnergyJni(BrowserWindowInterface* browser, float energy);

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_ANDROID_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_BRIDGE_H_

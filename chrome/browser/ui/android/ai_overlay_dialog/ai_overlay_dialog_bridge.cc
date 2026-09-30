// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/ai_overlay_dialog/ai_overlay_dialog_bridge.h"

#include "base/android/jni_android.h"
#include "build/android_buildflags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller_android.h"
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

// Must come after headers specializing FromJniType() / ToJniType().
#include "chrome/browser/ui/android/ai_overlay_dialog/jni_headers/AiOverlayDialogBridge_jni.h"

namespace ttc {

// Implements Java |AiOverlayDialogBridge.Natives#create|.
static int64_t JNI_AiOverlayDialogBridge_Create(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& caller,
    int64_t native_browser_window_ptr) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  auto* browser =
      reinterpret_cast<BrowserWindowInterface*>(native_browser_window_ptr);
  // Mirrors the guards BrowserWindowFeatures uses on desktop.
  if (!browser ||
      browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
    return 0;
  }
  return reinterpret_cast<intptr_t>(
      new AiOverlayDialogBridge(env, caller, browser));
#else
  return 0;
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

AiOverlayDialogBridge::AiOverlayDialogBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    BrowserWindowInterface* browser)
    : java_bridge_(env, java_bridge) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  controller_ = std::make_unique<AiOverlayDialogControllerAndroid>(browser);
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

AiOverlayDialogBridge::~AiOverlayDialogBridge() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_bridge =
      java_bridge_.get(env);
  if (java_bridge) {
    Java_AiOverlayDialogBridge_clearNativePtr(env, java_bridge);
  }
}

void AiOverlayDialogBridge::Destroy(JNIEnv* env) {
  delete this;
}

// The Java class ships in every Android APK (toolbar code references it), so
// this JNI entry point must be linked everywhere. The overlay itself only
// exists on Desktop Android, where AiOverlayDialogController is available; on
// other Android builds the feature flag is off and this is a no-op.
static void JNI_AiOverlayDialogBridge_ToggleOverlay(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_web_contents) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents) {
    return;
  }

  // The Java caller passes a tab's WebContents, but native cannot verify that,
  // and GetFromContents() null-derefs rather than returning null for a non-tab.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }

  // Owned by the Java AiOverlayDialogBridge's native counterpart; only present
  // for normal windows with the feature enabled.
  if (auto* controller = AiOverlayDialogController::From(browser)) {
    controller->ToggleOverlay();
  }
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

void UpdateAudioEnergyJni(BrowserWindowInterface* browser, float energy) {
  if (!browser || !browser->GetWindow()) {
    return;
  }
  auto* window =
      static_cast<ui::WindowAndroid*>(browser->GetWindow()->GetNativeWindow());
  if (!window) {
    return;
  }

  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_AiOverlayDialogBridge_updateAudioEnergy(env, window->GetJavaObject(),
                                               energy);
}

}  // namespace ttc

DEFINE_JNI(AiOverlayDialogBridge)

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/strip_tab_underline_manager.h"

#include "chrome/android/chrome_jni_headers/StripTabUnderlineManager_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

namespace android {

class StripTabUnderlineManager::UiDelegateImpl
    : public glic::TabUnderlineController::UiDelegate {
 public:
  UiDelegateImpl(StripTabUnderlineManager* manager, int tab_id)
      : manager_(manager), tab_id_(tab_id) {}
  ~UiDelegateImpl() override = default;

  void Show() override {
    is_showing_ = true;
    manager_->SetUnderlineState(tab_id_, true);
  }

  void StopShowing() override {
    is_showing_ = false;
    manager_->SetUnderlineState(tab_id_, false);
  }

  void ResetAnimationCycle() override {
    // No-op for Android as animation is not supported yet.
  }

  void StartRampingDown() override {
    // Fallback to StopShowing for Android as animation is not supported yet.
    StopShowing();
  }

  bool IsShowing() const override { return is_showing_; }

 private:
  raw_ptr<StripTabUnderlineManager> manager_;
  int tab_id_;
  bool is_showing_ = false;
};

StripTabUnderlineManager::StripTabUnderlineManager(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : java_obj_(env, obj) {}

StripTabUnderlineManager::~StripTabUnderlineManager() = default;

StripTabUnderlineManager::TabUnderlineContext::TabUnderlineContext() = default;

StripTabUnderlineManager::TabUnderlineContext::~TabUnderlineContext() = default;

StripTabUnderlineManager::TabUnderlineContext::TabUnderlineContext(
    std::unique_ptr<glic::TabUnderlineController> c,
    std::unique_ptr<UiDelegateImpl> d)
    : controller(std::move(c)), delegate(std::move(d)) {}

void StripTabUnderlineManager::Destroy(JNIEnv* env) {
  delete this;
}

void StripTabUnderlineManager::RegisterTab(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jtab) {
  // TODO(crbug.com/500128552): Maybe switch to the UserDataHost pattern instead
  // of maintaining a map here.
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (!tab_android) {
    return;
  }

  int tab_id = tab_android->GetAndroidId();

  // If already exists, do nothing.
  if (tracked_tabs_.find(tab_id) != tracked_tabs_.end()) {
    return;
  }

  auto controller =
      std::make_unique<glic::TabUnderlineController>(tab_android->GetHandle());
  auto delegate = std::make_unique<UiDelegateImpl>(this, tab_id);

  controller->Initialize(delegate.get(),
                         tab_android->GetBrowserWindowInterface());

  // Call OnUiReady() so it starts observing
  controller->OnUiReady();

  tracked_tabs_.try_emplace(tab_id, std::move(controller), std::move(delegate));
}

void StripTabUnderlineManager::UnregisterTab(JNIEnv* env, int32_t tab_id) {
  tracked_tabs_.erase(tab_id);
}

void StripTabUnderlineManager::SetUnderlineState(int tab_id,
                                                 bool is_underlined) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StripTabUnderlineManager_setUnderlineState(env, java_obj_, tab_id,
                                                  is_underlined);
}

static int64_t JNI_StripTabUnderlineManager_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new StripTabUnderlineManager(env, obj));
}

}  // namespace android

DEFINE_JNI(StripTabUnderlineManager)

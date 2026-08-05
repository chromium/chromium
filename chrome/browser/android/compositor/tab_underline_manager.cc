// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_underline_manager.h"

#include "chrome/android/chrome_jni_headers/TabUnderlineManager_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

namespace android {

class TabUnderlineManager::UiDelegateImpl
    : public glic::TabUnderlineController::UiDelegate {
 public:
  UiDelegateImpl(TabUnderlineManager* manager, int tab_id)
      : manager_(manager), tab_id_(tab_id) {}
  ~UiDelegateImpl() override = default;

  void Show() override {
    is_showing_ = true;
    manager_->SetUnderlineState(tab_id_, /*is_underlined=*/true);
  }

  void StopShowing() override {
    is_showing_ = false;
    manager_->SetUnderlineState(tab_id_, /*is_underlined=*/false);
  }

  void ResetAnimationCycle() override {
    manager_->ResetAnimationCycle(tab_id_);
  }

  void StartRampingDown() override {
    // Fallback to StopShowing for Android as animation is not supported yet.
    StopShowing();
  }

  bool IsShowing() const override { return is_showing_; }

 private:
  raw_ptr<TabUnderlineManager> manager_;
  int tab_id_;
  bool is_showing_ = false;
};

TabUnderlineManager::TabUnderlineManager(JNIEnv* env,
                                         const jni_zero::JavaRef<jobject>& obj)
    : java_obj_(env, obj) {}

TabUnderlineManager::~TabUnderlineManager() = default;

TabUnderlineManager::TabIndicatorContext::TabIndicatorContext() = default;

TabUnderlineManager::TabIndicatorContext::~TabIndicatorContext() = default;

TabUnderlineManager::TabIndicatorContext::TabIndicatorContext(
    std::unique_ptr<glic::TabUnderlineController> c,
    std::unique_ptr<UiDelegateImpl> d)
    : delegate(std::move(d)), controller(std::move(c)) {}

void TabUnderlineManager::Destroy(JNIEnv* env) {
  delete this;
}

void TabUnderlineManager::RegisterTab(JNIEnv* env,
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

  controller->Initialize(delegate.get());

  // Call OnUiReady() so it starts observing
  controller->OnUiReady();

  tracked_tabs_.try_emplace(tab_id, std::move(controller), std::move(delegate));
}

void TabUnderlineManager::UnregisterTab(JNIEnv* env, int32_t tab_id) {
  tracked_tabs_.erase(tab_id);
}

void TabUnderlineManager::SetUnderlineState(int tab_id, bool is_underlined) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabUnderlineManager_setUnderlineState(env, java_obj_, tab_id,
                                             is_underlined);
}

void TabUnderlineManager::ResetAnimationCycle(int tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabUnderlineManager_resetAnimationCycle(env, java_obj_, tab_id);
}

static int64_t JNI_TabUnderlineManager_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TabUnderlineManager(env, obj));
}

}  // namespace android

DEFINE_JNI(TabUnderlineManager)

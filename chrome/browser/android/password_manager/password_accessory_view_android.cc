// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_manager/password_accessory_view_android.h"

#include <jni.h>

#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "jni/PasswordAccessoryBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

PasswordAccessoryViewAndroid::PasswordAccessoryViewAndroid(
    PasswordAccessoryController* controller)
    : controller_(controller) {
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  java_object_.Reset(Java_PasswordAccessoryBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject()));
}

PasswordAccessoryViewAndroid::~PasswordAccessoryViewAndroid() {
  DCHECK(!java_object_.is_null());
  Java_PasswordAccessoryBridge_destroy(base::android::AttachCurrentThread(),
                                       java_object_);
  java_object_.Reset(nullptr);
}

void PasswordAccessoryViewAndroid::OnItemsAvailable(
    const std::vector<AccessoryItem>& items) {
  DCHECK(!java_object_.is_null());

  std::vector<base::string16> texts;
  std::vector<base::string16> descriptions;
  std::vector<int> password_stats;
  std::vector<int> item_types;
  texts.reserve(items.size());
  descriptions.reserve(items.size());
  password_stats.reserve(items.size());
  item_types.reserve(items.size());
  for (const auto& item : items) {
    texts.emplace_back(item.text);
    descriptions.emplace_back(item.content_description);
    password_stats.emplace_back(item.is_password);
    item_types.emplace_back(static_cast<int>(item.itemType));
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordAccessoryBridge_onItemsAvailable(
      env, java_object_, base::android::ToJavaArrayOfStrings(env, texts),
      base::android::ToJavaArrayOfStrings(env, descriptions),
      base::android::ToJavaIntArray(env, password_stats),
      base::android::ToJavaIntArray(env, item_types));
}

void PasswordAccessoryViewAndroid::CloseAccessorySheet() {
  Java_PasswordAccessoryBridge_closeAccessorySheet(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordAccessoryViewAndroid::SwapSheetWithKeyboard() {
  Java_PasswordAccessoryBridge_swapSheetWithKeyboard(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordAccessoryViewAndroid::ShowWhenKeyboardIsVisible() {
  Java_PasswordAccessoryBridge_showWhenKeyboardIsVisible(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordAccessoryViewAndroid::Hide() {
  Java_PasswordAccessoryBridge_hide(base::android::AttachCurrentThread(),
                                    java_object_);
}

void PasswordAccessoryViewAndroid::OnAutomaticGenerationStatusChanged(
    bool available) {
  if (!available && java_object_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordAccessoryBridge_onAutomaticGenerationStatusChanged(
      env, java_object_, available);
}

void PasswordAccessoryViewAndroid::OnFaviconRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint desiredSizeInPx,
    const base::android::JavaParamRef<jobject>& j_callback) {
  controller_->GetFavicon(
      desiredSizeInPx,
      base::BindOnce(&PasswordAccessoryViewAndroid::OnImageFetched,
                     base::Unretained(this),  // Outlives or cancels request.
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void PasswordAccessoryViewAndroid::OnFillingTriggered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean isPassword,
    const base::android::JavaParamRef<_jstring*>& textToFill) {
  controller_->OnFillingTriggered(
      isPassword, base::android::ConvertJavaStringToUTF16(textToFill));
}

void PasswordAccessoryViewAndroid::OnOptionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<_jstring*>& selectedOption) {
  controller_->OnOptionSelected(
      base::android::ConvertJavaStringToUTF16(selectedOption));
}

void PasswordAccessoryViewAndroid::OnGenerationRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnGenerationRequested();
}

void PasswordAccessoryViewAndroid::OnImageFetched(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    const gfx::Image& image) {
  base::android::ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty())
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());

  RunObjectCallbackAndroid(j_callback, j_bitmap);
}

// static
std::unique_ptr<PasswordAccessoryViewInterface>
PasswordAccessoryViewInterface::Create(
    PasswordAccessoryController* controller) {
  return std::make_unique<PasswordAccessoryViewAndroid>(controller);
}

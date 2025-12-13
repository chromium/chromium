// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_group_collection_data_android.h"

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from TabGroupCollectionData.java.
#include "chrome/browser/tab/jni_headers/TabGroupCollectionData_jni.h"

namespace tabs {

TabGroupCollectionDataAndroid::TabGroupCollectionDataAndroid(
    std::unique_ptr<TabGroupCollectionData> data)
    : data_(std::move(data)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_object_.Reset(
      Java_TabGroupCollectionData_init(env, reinterpret_cast<intptr_t>(this)));
}

TabGroupCollectionDataAndroid::~TabGroupCollectionDataAndroid() = default;

void TabGroupCollectionDataAndroid::Destroy(JNIEnv* env) {
  delete this;
}

base::Token TabGroupCollectionDataAndroid::GetTabGroupId(JNIEnv* env) const {
  return data_->tab_group_id_;
}

const std::u16string& TabGroupCollectionDataAndroid::GetTitle(
    JNIEnv* env) const {
  return data_->title_;
}

tab_groups::TabGroupColorId TabGroupCollectionDataAndroid::GetColor(
    JNIEnv* env) {
  return data_->color_;
}

bool TabGroupCollectionDataAndroid::IsCollapsed(JNIEnv* env) const {
  return data_->is_collapsed_;
}

jni_zero::ScopedJavaLocalRef<jobject>
TabGroupCollectionDataAndroid::GetJavaObject() const {
  return j_object_;
}

}  // namespace tabs

DEFINE_JNI(TabGroupCollectionData)

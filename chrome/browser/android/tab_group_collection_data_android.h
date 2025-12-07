// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_GROUP_COLLECTION_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_GROUP_COLLECTION_DATA_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/token.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "components/tab_groups/tab_group_color.h"
#include "third_party/jni_zero/jni_zero.h"

namespace tabs {

// Android wrapper for TabGroupCollectionData, used to access data
// from Java.
class TabGroupCollectionDataAndroid {
 public:
  explicit TabGroupCollectionDataAndroid(
      std::unique_ptr<TabGroupCollectionData> data);
  ~TabGroupCollectionDataAndroid();

  TabGroupCollectionDataAndroid(const TabGroupCollectionDataAndroid&) = delete;
  TabGroupCollectionDataAndroid& operator=(
      const TabGroupCollectionDataAndroid&) = delete;

  // This C++ object is owned by the Java counterpart, and should be destroyed
  // by it.
  void Destroy(JNIEnv* env);
  jni_zero::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  base::Token GetTabGroupId(JNIEnv* env) const;
  const std::u16string& GetTitle(JNIEnv* env) const;
  bool IsCollapsed(JNIEnv* env) const;
  tab_groups::TabGroupColorId GetColor(JNIEnv* env);

 private:
  std::unique_ptr<TabGroupCollectionData> data_;
  jni_zero::ScopedJavaLocalRef<jobject> j_object_;
};

}  // namespace tabs

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<tabs::TabGroupCollectionDataAndroid>(
    JNIEnv* env,
    const tabs::TabGroupCollectionDataAndroid& input) {
  return input.GetJavaObject();
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_ANDROID_TAB_GROUP_COLLECTION_DATA_ANDROID_H_

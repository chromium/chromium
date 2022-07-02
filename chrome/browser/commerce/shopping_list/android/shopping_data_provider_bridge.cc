// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/commerce/shopping_list/jni_headers/ShoppingDataProviderBridge_jni.h"
#include "chrome/browser/commerce/shopping_list/shopping_data_provider.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace shopping_list {

namespace android {

ScopedJavaLocalRef<jbyteArray> JNI_ShoppingDataProviderBridge_GetForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  shopping_list::ShoppingDataProvider* data_provider =
      shopping_list::ShoppingDataProvider::FromWebContents(web_contents);

  if (!data_provider)
    return nullptr;

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      data_provider->GetCurrentMetadata();

  if (!meta)
    return nullptr;

  int size = meta->ByteSize();
  std::vector<uint8_t> data(size);
  meta->SerializeToArray(data.data(), size);
  return base::android::ToJavaByteArray(env, data.data(), size);
}

}  // namespace android

}  // namespace shopping_list

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/commerce/android/shopping_service_jni/PriceTrackingUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace commerce {

void JNI_PriceTrackingUtils_SetPriceTrackingStateForBookmark(
    JNIEnv* env,
    Profile* profile,
    jlong bookmark_id,
    jboolean enabled,
    const JavaParamRef<jobject>& j_callback,
    jboolean bookmark_created_by_price_tracking) {
  CHECK(profile);

  ShoppingService* service =
      ShoppingServiceFactory::GetForBrowserContext(profile);
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  CHECK(service);
  CHECK(model);

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, bookmark_id);

  SetPriceTrackingStateForBookmark(
      service, model, node, enabled,
      base::BindOnce(
          [](const ScopedJavaGlobalRef<jobject>& callback, bool success) {
            base::android::RunBooleanCallbackAndroid(callback, success);
          },
          ScopedJavaGlobalRef<jobject>(j_callback)),
      bookmark_created_by_price_tracking);
}

void JNI_PriceTrackingUtils_IsBookmarkPriceTracked(
    JNIEnv* env,
    Profile* profile,
    jlong bookmark_id,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(profile);

  ShoppingService* service =
      ShoppingServiceFactory::GetForBrowserContext(profile);
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  CHECK(service);
  CHECK(model);

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, bookmark_id);

  IsBookmarkPriceTracked(
      service, model, node,
      base::BindOnce(
          [](const ScopedJavaGlobalRef<jobject>& callback, bool is_tracked) {
            base::android::RunBooleanCallbackAndroid(callback, is_tracked);
          },
          ScopedJavaGlobalRef<jobject>(j_callback)));
}

}  // namespace commerce

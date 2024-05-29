// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/android/shopping_service_android.h"
#include "components/commerce/core/shopping_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/commerce/android/shopping_service_jni/ShoppingServiceFactory_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {
const char kShoppingServiceBridgeKey[] = "shopping-service-jni-bridge-key";
}

namespace commerce {

ScopedJavaLocalRef<jobject> JNI_ShoppingServiceFactory_GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile);

  ShoppingService* service =
      ShoppingServiceFactory::GetForBrowserContext(profile);
  CHECK(service);

  ShoppingServiceAndroid* bridge = static_cast<ShoppingServiceAndroid*>(
      service->GetUserData(kShoppingServiceBridgeKey));
  if (!bridge) {
    bridge = new ShoppingServiceAndroid(service);
    service->SetUserData(kShoppingServiceBridgeKey, base::WrapUnique(bridge));
  }

  CHECK(bridge);

  return ScopedJavaLocalRef<jobject>(bridge->java_ref());
}

}  // namespace commerce

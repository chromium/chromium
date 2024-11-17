// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/collaboration/internal/android/messaging/messaging_backend_service_bridge.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/collaboration/messaging_jni_headers/MessagingBackendServiceFactory_jni.h"

namespace collaboration::messaging::android {

static base::android::ScopedJavaLocalRef<jobject>
JNI_MessagingBackendServiceFactory_GetForProfile(JNIEnv* env,
                                                 Profile* profile) {
  CHECK(profile);
  MessagingBackendService* service =
      MessagingBackendServiceFactory::GetForProfile(profile);
  CHECK(service);

  return MessagingBackendServiceBridge::GetBridgeForMessagingBackendService(
      service);
}

}  // namespace collaboration::messaging::android

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/messaging/messaging_backend_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/saved_tab_groups/messaging/android/messaging_backend_service_bridge.h"
#include "components/saved_tab_groups/messaging/messaging_backend_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_sync/messaging/android/jni_headers/MessagingBackendServiceFactory_jni.h"

namespace tab_groups::messaging::android {

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

}  // namespace tab_groups::messaging::android

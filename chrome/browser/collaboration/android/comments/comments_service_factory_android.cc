// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/comments/comments_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/collaboration/internal/android/comments/comments_service_bridge.h"
#include "components/collaboration/public/comments/comments_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/collaboration/comments_jni_headers/CommentsServiceFactory_jni.h"

namespace collaboration::comments::android {

static base::android::ScopedJavaLocalRef<jobject>
JNI_CommentsServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  CHECK(profile);
  CommentsService* service = CommentsServiceFactory::GetForProfile(profile);
  CHECK(service);

  return CommentsServiceBridge::GetBridgeForCommentsService(service);
}

}  // namespace collaboration::comments::android

DEFINE_JNI(CommentsServiceFactory)

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NavigationRecorder_jni.h"

using base::android::JavaRef;

namespace feed::android {
namespace {
FeedApi* GetFeedApi(Profile* profile) {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(profile);
  return service ? service->GetStream() : nullptr;
}

SurfaceId FromJavaSurfaceId(int32_t surface_id) {
  return feed::SurfaceId::FromUnsafeValue(surface_id);
}

}  // namespace

// Records how long a user visits a Feeds card when the Tab was foreground.
static void JNI_NavigationRecorder_ReportOpenVisitComplete(
    JNIEnv* env,
    Profile* profile,
    int32_t surface_id,
    int64_t visitTimeMs) {
  FeedApi* api = GetFeedApi(profile);
  if (!api) {
    return;
  }
  api->ReportOpenVisitComplete(FromJavaSurfaceId(surface_id),
                               base::Milliseconds(visitTimeMs));
}

}  // namespace feed::android

DEFINE_JNI(NavigationRecorder)

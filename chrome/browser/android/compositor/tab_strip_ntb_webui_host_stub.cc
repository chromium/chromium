// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NtbWebUiCoordinator_jni.h"

namespace android {

class TabStripNtbWebUiHost {
 public:
  void StartLoad() { NOTREACHED(); }
  content::WebContents* GetWebContents() { NOTREACHED(); }
  void Resize(int32_t width_px, int32_t height_px) { NOTREACHED(); }
  void SetButtonVisible(bool visible) { NOTREACHED(); }
};

static jni_zero::JniUniquePtr<TabStripNtbWebUiHost>
JNI_NtbWebUiCoordinator_Init(const base::android::JavaRef<jobject>& caller,
                             Profile* profile,
                             int64_t activity_creation_uptime_ms) {
  return nullptr;
}

static void JNI_NtbWebUiCoordinator_SetTabModel(TabStripNtbWebUiHost* host,
                                                int32_t session_id) {
  NOTREACHED();
}

}  // namespace android

DEFINE_JNI(NtbWebUiCoordinator)

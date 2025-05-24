// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_ANDROID_OFFSET_TAG_ANDROID_H_
#define CC_INPUT_ANDROID_OFFSET_TAG_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "components/viz/common/quads/offset_tag.h"

namespace cc::android {

CC_EXPORT viz::OffsetTag FromJavaOffsetTag(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& joffset_tag);

CC_EXPORT BrowserControlsOffsetTags FromJavaBrowserControlsOffsetTags(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbrowser_controls_offset_tags);

CC_EXPORT BrowserControlsOffsetTagModifications
FromJavaBrowserControlsOffsetTagModifications(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        jbrowser_controls_offset_tag_modifications);

}  // namespace cc::android

#endif  // CC_INPUT_ANDROID_OFFSET_TAG_ANDROID_H_

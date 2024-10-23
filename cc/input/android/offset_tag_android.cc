// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/android/offset_tag_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/android/token_android.h"
#include "base/logging.h"
#include "base/token.h"
#include "cc/cc_jni_headers/BrowserControlsOffsetTagsInfo_jni.h"
#include "cc/cc_jni_headers/OffsetTag_jni.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "components/viz/common/quads/offset_tag.h"

namespace cc::android {

viz::OffsetTag FromJavaOffsetTag(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& joffset_tag) {
  if (!joffset_tag) {
    return viz::OffsetTag();
  }
  const base::android::JavaRef<jobject>& jtoken =
      Java_OffsetTag_getToken(env, joffset_tag);
  base::Token token = base::android::TokenAndroid::FromJavaToken(env, jtoken);
  return viz::OffsetTag(token);
}

cc::BrowserControlsOffsetTagsInfo FromJavaBrowserControlsOffsetTagsInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbrowser_controls_offset_tags_info) {
  cc::BrowserControlsOffsetTagsInfo tags_info;
  if (!jbrowser_controls_offset_tags_info) {
    return tags_info;
  }

  const base::android::JavaRef<jobject>& jcontent_offset_tag =
      Java_BrowserControlsOffsetTagsInfo_getContentOffsetTag(
          env, jbrowser_controls_offset_tags_info);
  viz::OffsetTag content_offset_tag =
      FromJavaOffsetTag(env, jcontent_offset_tag);
  tags_info.content_offset_tag = content_offset_tag;

  const base::android::JavaRef<jobject>& jtop_controls_offset_tag =
      Java_BrowserControlsOffsetTagsInfo_getTopControlsOffsetTag(
          env, jbrowser_controls_offset_tags_info);
  viz::OffsetTag top_controls_offset_tag =
      FromJavaOffsetTag(env, jtop_controls_offset_tag);
  tags_info.top_controls_offset_tag = top_controls_offset_tag;

  tags_info.top_controls_height =
      Java_BrowserControlsOffsetTagsInfo_getTopControlsHeight(
          env, jbrowser_controls_offset_tags_info);

  tags_info.top_controls_hairline_height =
      Java_BrowserControlsOffsetTagsInfo_getTopControlsHairlineHeight(
          env, jbrowser_controls_offset_tags_info);

  return tags_info;
}

}  // namespace cc::android

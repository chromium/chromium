// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/android/offset_tag_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/android/token_android.h"
#include "base/logging.h"
#include "base/token.h"
#include "cc/cc_jni_headers/BrowserControlsOffsetTagModifications_jni.h"
#include "cc/cc_jni_headers/BrowserControlsOffsetTags_jni.h"
#include "cc/cc_jni_headers/OffsetTag_jni.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
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

BrowserControlsOffsetTags FromJavaBrowserControlsOffsetTags(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbrowser_controls_offset_tags) {
  BrowserControlsOffsetTags offset_tags;
  if (!jbrowser_controls_offset_tags) {
    return offset_tags;
  }

  const base::android::JavaRef<jobject>& jtop_controls_offset_tag =
      Java_BrowserControlsOffsetTags_getTopControlsOffsetTag(
          env, jbrowser_controls_offset_tags);
  viz::OffsetTag top_controls_offset_tag =
      android::FromJavaOffsetTag(env, jtop_controls_offset_tag);
  offset_tags.top_controls_offset_tag = top_controls_offset_tag;

  const base::android::JavaRef<jobject>& jcontent_offset_tag =
      Java_BrowserControlsOffsetTags_getContentOffsetTag(
          env, jbrowser_controls_offset_tags);
  viz::OffsetTag content_offset_tag =
      android::FromJavaOffsetTag(env, jcontent_offset_tag);
  offset_tags.content_offset_tag = content_offset_tag;

  const base::android::JavaRef<jobject>& jbottom_controls_offset_tag =
      Java_BrowserControlsOffsetTags_getBottomControlsOffsetTag(
          env, jbrowser_controls_offset_tags);
  viz::OffsetTag bottom_controls_offset_tag =
      android::FromJavaOffsetTag(env, jbottom_controls_offset_tag);
  offset_tags.bottom_controls_offset_tag = bottom_controls_offset_tag;

  return offset_tags;
}

BrowserControlsOffsetTagModifications
FromJavaBrowserControlsOffsetTagModifications(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        jbrowser_controls_offset_tag_modifications) {
  BrowserControlsOffsetTagModifications offset_tag_modifications;
  if (!jbrowser_controls_offset_tag_modifications) {
    return offset_tag_modifications;
  }

  const base::android::JavaRef<jobject>& jtags =
      Java_BrowserControlsOffsetTagModifications_getTags(
          env, jbrowser_controls_offset_tag_modifications);
  BrowserControlsOffsetTags tags =
      FromJavaBrowserControlsOffsetTags(env, jtags);
  offset_tag_modifications.tags = tags;

  offset_tag_modifications.top_controls_additional_height =
      Java_BrowserControlsOffsetTagModifications_getTopControlsAdditionalHeight(
          env, jbrowser_controls_offset_tag_modifications);

  offset_tag_modifications.bottom_controls_additional_height =
      Java_BrowserControlsOffsetTagModifications_getBottomControlsAdditionalHeight(
          env, jbrowser_controls_offset_tag_modifications);

  return offset_tag_modifications;
}

}  // namespace cc::android

DEFINE_JNI(BrowserControlsOffsetTagModifications)
DEFINE_JNI(BrowserControlsOffsetTags)
DEFINE_JNI(OffsetTag)

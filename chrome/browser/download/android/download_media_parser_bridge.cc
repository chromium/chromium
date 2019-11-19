// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_media_parser_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/android/chrome_jni_headers/DownloadMediaData_jni.h"
#include "chrome/android/chrome_jni_headers/DownloadMediaParserBridge_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertUTF8ToJavaString;

namespace {

void OnMediaParsed(const base::android::ScopedJavaGlobalRef<jobject> jcallback,
                   bool success,
                   chrome::mojom::MediaMetadataPtr metadata,
                   SkBitmap thumbnail_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(metadata);

  // Copy the thumbnail bitmap to a Java Bitmap object.
  base::android::ScopedJavaLocalRef<jobject> java_bitmap;
  if (!thumbnail_bitmap.isNull())
    java_bitmap = gfx::ConvertToJavaBitmap(&thumbnail_bitmap);

  base::android::ScopedJavaLocalRef<jobject> media_data;
  if (success) {
    media_data = Java_DownloadMediaData_Constructor(
        env, metadata->duration, ConvertUTF8ToJavaString(env, metadata->title),
        ConvertUTF8ToJavaString(env, metadata->artist), std::move(java_bitmap));
  }

  base::android::RunObjectCallbackAndroid(jcallback, std::move(media_data));
}

}  // namespace

// static
jlong JNI_DownloadMediaParserBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jmime_type,
    const base::android::JavaParamRef<jstring>& jfile_path,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfile_path));
  std::string mime_type =
      base::android::ConvertJavaStringToUTF8(env, jmime_type);

  auto* bridge = new DownloadMediaParserBridge(
      mime_type, file_path,
      base::BindOnce(&OnMediaParsed,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
  return reinterpret_cast<intptr_t>(bridge);
}

DownloadMediaParserBridge::DownloadMediaParserBridge(
    const std::string& mime_type,
    const base::FilePath& file_path,
    DownloadMediaParser::ParseCompleteCB parse_complete_cb)
    : parser_(std::make_unique<DownloadMediaParser>(mime_type, file_path)),
      parse_complete_cb_(std::move(parse_complete_cb)) {}

DownloadMediaParserBridge::~DownloadMediaParserBridge() = default;

void DownloadMediaParserBridge::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void DownloadMediaParserBridge::Start(JNIEnv* env, jobject obj) {
  parser_->Start(std::move(parse_complete_cb_));
}

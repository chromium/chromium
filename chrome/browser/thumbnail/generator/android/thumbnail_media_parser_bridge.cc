// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/thumbnail/generator/test_support_jni_headers/ThumbnailMediaData_jni.h"
#include "chrome/browser/thumbnail/generator/test_support_jni_headers/ThumbnailMediaParserBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;

namespace {

void OnMediaParsed(ThumbnailMediaParser*,
                   const base::android::ScopedJavaGlobalRef<jobject> jcallback,
                   bool success,
                   chrome::mojom::MediaMetadataPtr metadata,
                   SkBitmap thumbnail_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(metadata);

  // Copy the thumbnail bitmap to a Java Bitmap object.
  base::android::ScopedJavaLocalRef<jobject> java_bitmap;
  if (!thumbnail_bitmap.isNull())
    java_bitmap = gfx::ConvertToJavaBitmap(thumbnail_bitmap);

  base::android::ScopedJavaLocalRef<jobject> media_data;
  if (success) {
    media_data = Java_ThumbnailMediaData_Constructor(
        env, metadata->duration, ConvertUTF8ToJavaString(env, metadata->title),
        ConvertUTF8ToJavaString(env, metadata->artist), std::move(java_bitmap));
  }

  base::android::RunObjectCallbackAndroid(jcallback, std::move(media_data));
}

}  // namespace

// static
void JNI_ThumbnailMediaParserBridge_Parse(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jmime_type,
    const base::android::JavaParamRef<jstring>& jfile_path,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfile_path));
  std::string mime_type =
      base::android::ConvertJavaStringToUTF8(env, jmime_type);

  // Deletes self
  ThumbnailMediaParser* parser =
      ThumbnailMediaParser::Create(mime_type, file_path).release();
  parser->Start(
      base::BindOnce(&OnMediaParsed, base::Owned(parser),
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

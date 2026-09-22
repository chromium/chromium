// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/thumbnail/generator/android/thumbnail_media_parser.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/thumbnail/generator/test_support_jni_headers/ThumbnailMediaData_jni.h"
#include "chrome/browser/thumbnail/generator/test_support_jni_headers/ThumbnailMediaParserBridge_jni.h"

namespace {

void OnMediaParsed(ThumbnailMediaParser*,
                   const jni_zero::ScopedJavaGlobalRef<jobject> jcallback,
                   bool success,
                   chrome::mojom::MediaMetadataPtr metadata,
                   SkBitmap thumbnail_bitmap) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  DCHECK(metadata);

  jni_zero::ScopedJavaLocalRef<jobject> media_data;
  if (success) {
    media_data = Java_ThumbnailMediaData_Constructor(
        env, metadata->duration, metadata->title, metadata->artist,
        thumbnail_bitmap);
  }

  base::android::RunObjectCallbackAndroid(jcallback, std::move(media_data));
}

}  // namespace

// static
static void JNI_ThumbnailMediaParserBridge_Parse(
    const std::string& mime_type,
    const std::string& file_path,
    const jni_zero::JavaRef<jobject>& jcallback) {
  // Deletes self
  ThumbnailMediaParser* parser =
      ThumbnailMediaParser::Create(mime_type, base::FilePath(file_path))
          .release();
  parser->Start(
      base::BindOnce(&OnMediaParsed, base::Owned(parser),
                     jni_zero::ScopedJavaGlobalRef<jobject>(jcallback)));
}

DEFINE_JNI(ThumbnailMediaData)
DEFINE_JNI(ThumbnailMediaParserBridge)

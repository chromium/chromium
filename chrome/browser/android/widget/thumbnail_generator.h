// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WIDGET_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_ANDROID_WIDGET_THUMBNAIL_GENERATOR_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/image_thumbnail_request.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

class DownloadMediaParser;

// Kicks off asynchronous pipelines for creating thumbnails for local files.
// The native-side ThumbnailGenerator is owned by the Java-side and can be
// safely destroyed while a request is being processed.
class ThumbnailGenerator {
 public:
  explicit ThumbnailGenerator(const base::android::JavaParamRef<jobject>& jobj);

  // Destroys the ThumbnailGenerator.  Any currently running ImageRequest will
  // delete itself when it has completed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Kicks off an asynchronous process to retrieve the thumbnail for the file
  // located at |file_path| with a max size of |icon_size| in each dimension.
  // Invokes the Java #onthumbnailRetrieved(String, int, Bitmap, boolean) method
  // when finished.
  void RetrieveThumbnail(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jcontent_id,
      const base::android::JavaParamRef<jstring>& jfile_path,
      const base::android::JavaParamRef<jstring>& jmime_type,
      jint icon_size,
      const base::android::JavaParamRef<jobject>& callback);

 private:
  ~ThumbnailGenerator();

  // Called when the image thumbnail is ready.  |thumbnail| will be empty on
  // failure.
  void OnImageThumbnailRetrieved(
      base::OnceCallback<void(SkBitmap)> java_callback,
      const SkBitmap& thumbnail);

  // Called when the video thumbnail is ready.  |thumbnail| will be empty on
  // failure.
  void OnVideoThumbnailRetrieved(
      base::OnceCallback<void(SkBitmap)> java_callback,
      int icon_size,
      std::unique_ptr<DownloadMediaParser> parser,
      bool success,
      chrome::mojom::MediaMetadataPtr media_metadata,
      SkBitmap thumbnail);

  // This is a {@link ThumbnailGenerator} Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::WeakPtrFactory<ThumbnailGenerator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_ANDROID_WIDGET_THUMBNAIL_GENERATOR_H_

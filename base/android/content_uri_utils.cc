// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/content_uri_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/content_uri_utils_jni/ContentUriUtils_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace base {

bool ContentUriExists(const FilePath& content_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());
  return Java_ContentUriUtils_contentUriExists(env, j_uri);
}

std::optional<std::string> TranslateOpenFlagsToJavaMode(uint32_t open_flags) {
  // The allowable modes from ParcelFileDescriptor#parseMode() are
  // ("r", "w", "wt", "wa", "rw", "rwt"), we disallow "w" which has been the
  // source of android security issues.

  // Ignore async.
  open_flags &= ~File::FLAG_ASYNC;

  switch (open_flags) {
    case File::FLAG_OPEN | File::FLAG_READ:
      return "r";
    case File::FLAG_OPEN_ALWAYS | File::FLAG_READ | File::FLAG_WRITE:
      return "rw";
    case File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND:
      return "wa";
    case File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE:
      return "rwt";
    case File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE:
      return "wt";
    default:
      return std::nullopt;
  }
}

File OpenContentUri(const FilePath& content_uri, uint32_t open_flags) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());
  auto mode = TranslateOpenFlagsToJavaMode(open_flags);
  CHECK(mode.has_value()) << "Unsupported flags=0x" << std::hex << open_flags;
  ScopedJavaLocalRef<jstring> j_mode =
      ConvertUTF8ToJavaString(env, mode.value());
  jint fd = Java_ContentUriUtils_openContentUri(env, j_uri, j_mode);
  if (fd < 0)
    return File();
  return File(fd);
}

int64_t GetContentUriFileSize(const FilePath& content_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());
  return Java_ContentUriUtils_getContentUriFileSize(env, j_uri);
}

std::string GetContentUriMimeType(const FilePath& content_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());
  ScopedJavaLocalRef<jstring> j_mime =
      Java_ContentUriUtils_getMimeType(env, j_uri);
  if (j_mime.is_null())
    return std::string();

  return base::android::ConvertJavaStringToUTF8(env, j_mime.obj());
}

bool MaybeGetFileDisplayName(const FilePath& content_uri,
                             std::u16string* file_display_name) {
  if (!content_uri.IsContentUri())
    return false;

  DCHECK(file_display_name);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());
  ScopedJavaLocalRef<jstring> j_display_name =
      Java_ContentUriUtils_maybeGetDisplayName(env, j_uri);

  if (j_display_name.is_null())
    return false;

  *file_display_name = base::android::ConvertJavaStringToUTF16(j_display_name);
  return true;
}

bool DeleteContentUri(const FilePath& content_uri) {
  DCHECK(content_uri.IsContentUri());
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      ConvertUTF8ToJavaString(env, content_uri.value());

  return Java_ContentUriUtils_delete(env, j_uri);
}

}  // namespace base

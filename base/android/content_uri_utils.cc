// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/content_uri_utils.h"

#include <sys/stat.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/content_uri_utils_jni/ContentUriUtils_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace base {
namespace {
std::string SafeConvertJavaStringToUTF8(JNIEnv* env,
                                        const JavaRef<jstring>& str) {
  if (str.is_null()) {
    return std::string();
  }
  return base::android::ConvertJavaStringToUTF8(env, str);
}
}  // namespace

namespace internal {

bool ContentUriExists(const FilePath& content_uri) {
  JNIEnv* env = android::AttachCurrentThread();
  return Java_ContentUriUtils_contentUriExists(env, content_uri.value());
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

int OpenContentUri(const FilePath& content_uri, uint32_t open_flags) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto mode = TranslateOpenFlagsToJavaMode(open_flags);
  CHECK(mode.has_value()) << "Unsupported flags=0x" << std::hex << open_flags;
  return Java_ContentUriUtils_openContentUri(env, content_uri.value(), *mode);
}

bool ContentUriGetFileInfo(const FilePath& content_uri, File::Info* results) {
  JNIEnv* env = android::AttachCurrentThread();
  std::vector<FileEnumerator::FileInfo> result;
  Java_ContentUriUtils_getFileInfo(env, content_uri.value(),
                                   reinterpret_cast<jlong>(&result));
  // Java will call back sync to AddFileInfoToVector(&result).
  if (result.empty()) {
    return false;
  }
  // Android can return -1 for unknown size, which
  // we can't deal with, so we will consider that the file wasn't found.
  if (result[0].GetSize() < 0) {
    LOG(ERROR) << "Unknown file length for " << content_uri;
    return false;
  }
  results->size = result[0].GetSize();
  results->is_directory = result[0].IsDirectory();
  results->last_modified = result[0].GetLastModifiedTime();
  return true;
}

std::vector<FileEnumerator::FileInfo> ListContentUriDirectory(
    const FilePath& content_uri) {
  JNIEnv* env = android::AttachCurrentThread();
  std::vector<FileEnumerator::FileInfo> result;
  Java_ContentUriUtils_listDirectory(env, content_uri.value(),
                                     reinterpret_cast<jlong>(&result));
  // Java will call back sync to AddFileInfoToVector(&result).
  return result;
}

bool DeleteContentUri(const FilePath& content_uri) {
  DCHECK(content_uri.IsContentUri());
  JNIEnv* env = android::AttachCurrentThread();
  return Java_ContentUriUtils_delete(env, content_uri.value());
}

}  // namespace internal

void JNI_ContentUriUtils_AddFileInfoToVector(
    JNIEnv* env,
    jlong vector_pointer,
    const JavaParamRef<jstring>& uri,
    const JavaParamRef<jstring>& display_name,
    jboolean is_directory,
    jlong size,
    jlong last_modified) {
  auto* result =
      reinterpret_cast<std::vector<FileEnumerator::FileInfo>*>(vector_pointer);
  result->emplace_back(FilePath(SafeConvertJavaStringToUTF8(env, uri)),
                       FilePath(SafeConvertJavaStringToUTF8(env, display_name)),
                       is_directory, size,
                       Time::FromMillisecondsSinceUnixEpoch(last_modified));
}

std::string GetContentUriMimeType(const FilePath& content_uri) {
  JNIEnv* env = android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime =
      Java_ContentUriUtils_getMimeType(env, content_uri.value());
  return SafeConvertJavaStringToUTF8(env, j_mime);
}

bool MaybeGetFileDisplayName(const FilePath& content_uri,
                             std::u16string* file_display_name) {
  if (!content_uri.IsContentUri())
    return false;

  DCHECK(file_display_name);

  JNIEnv* env = android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_display_name =
      Java_ContentUriUtils_maybeGetDisplayName(env, content_uri.value());

  if (j_display_name.is_null())
    return false;

  *file_display_name = android::ConvertJavaStringToUTF16(j_display_name);
  return true;
}

FilePath ContentUriBuildDocumentUriUsingTree(
    const FilePath& tree_uri,
    const std::string& encoded_document_id) {
  JNIEnv* env = android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_uri =
      Java_ContentUriUtils_buildDocumentUriUsingTree(env, tree_uri.value(),
                                                     encoded_document_id);
  return FilePath(SafeConvertJavaStringToUTF8(env, j_uri));
}

}  // namespace base

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_info.h"

#include "base/android/jni_array.h"
#include "chrome/android/chrome_jni_headers/HistoryDeletionInfo_jni.h"
#include "components/history/core/browser/history_types.h"

using base::android::ScopedJavaLocalRef;

namespace {

history::DeletionInfo* ToDeletionInfo(jlong j_deletion_info) {
  return reinterpret_cast<history::DeletionInfo*>(j_deletion_info);
}

}  // namespace

ScopedJavaLocalRef<jobjectArray> JNI_HistoryDeletionInfo_GetDeletedURLs(
    JNIEnv* env,
    jlong history_deletion_info_ptr) {
  history::DeletionInfo* deletion_info =
      ToDeletionInfo(history_deletion_info_ptr);
  std::vector<std::string> deleted_urls;
  for (auto row : deletion_info->deleted_rows()) {
    deleted_urls.push_back(row.url().spec());
  }

  return base::android::ToJavaArrayOfStrings(env, deleted_urls);
}

jboolean JNI_HistoryDeletionInfo_IsTimeRangeValid(
    JNIEnv* env,
    jlong history_deletion_info_ptr) {
  history::DeletionInfo* deletion_info =
      ToDeletionInfo(history_deletion_info_ptr);
  return deletion_info->time_range().IsValid();
}

jboolean JNI_HistoryDeletionInfo_IsTimeRangeForAllTime(
    JNIEnv* env,
    jlong history_deletion_info_ptr) {
  history::DeletionInfo* deletion_info =
      ToDeletionInfo(history_deletion_info_ptr);
  return deletion_info->time_range().IsAllTime();
}

jlong JNI_HistoryDeletionInfo_GetTimeRangeBegin(
    JNIEnv* env,
    jlong history_deletion_info_ptr) {
  history::DeletionInfo* deletion_info =
      ToDeletionInfo(history_deletion_info_ptr);
  return deletion_info->time_range().begin().ToJavaTime();
}

jlong JNI_HistoryDeletionInfo_GetTimeRangeEnd(JNIEnv* env,
                                              jlong history_deletion_info_ptr) {
  history::DeletionInfo* deletion_info =
      ToDeletionInfo(history_deletion_info_ptr);
  return deletion_info->time_range().end().ToJavaTime();
}

ScopedJavaLocalRef<jobject> CreateHistoryDeletionInfo(
    JNIEnv* env,
    const history::DeletionInfo* deletion_info) {
  return Java_HistoryDeletionInfo_create(
      env, reinterpret_cast<intptr_t>(deletion_info));
}

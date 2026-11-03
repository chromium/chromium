// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_info.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "components/history/core/browser/history_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/HistoryDeletionInfo_jni.h"

using base::android::ScopedJavaLocalRef;

ScopedJavaLocalRef<jobject> CreateHistoryDeletionInfo(
    JNIEnv* env,
    const history::DeletionInfo* deletion_info) {
  std::vector<std::string> deleted_urls;
  for (const auto& row : deletion_info->deleted_rows()) {
    deleted_urls.push_back(row.url().spec());
  }

  return Java_HistoryDeletionInfo_create(
      env, base::android::ToJavaArrayOfStrings(env, deleted_urls),
      deletion_info->time_range().IsValid(),
      deletion_info->time_range().IsAllTime());
}

DEFINE_JNI(HistoryDeletionInfo)

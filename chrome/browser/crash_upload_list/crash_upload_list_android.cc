// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros_local.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MinidumpUploadServiceImpl_jni.h"

namespace {

enum class UnsuccessfulUploadListState {
  FORCED,
  PENDING,
  NOT_UPLOADED,
  OTHER_FILENAME_SUFFIX,
  FAILED_TO_LOAD_FILE_INFO,
  FAILED_TO_LOAD_FILE_SIZE,
  FAILED_TO_FIND_DASH,
  ADDING_AN_UPLOAD_ENTRY,
  COUNT
};

// TODO(isherman): This is a temporary histogram for debugging
// [ https://crbug.com/772159 ] and should be removed once that bug is closed.
void RecordUnsuccessfulUploadListState(UnsuccessfulUploadListState state) {
  LOCAL_HISTOGRAM_ENUMERATION(
      "Debug.Crash.Android.LoadUnsuccessfulUploadListState", state,
      UnsuccessfulUploadListState::COUNT);
}

}  // namespace

CrashUploadListAndroid::CrashUploadListAndroid(
    const base::FilePath& upload_log_path)
    : TextLogUploadList(upload_log_path) {}

CrashUploadListAndroid::~CrashUploadListAndroid() {}

// static
bool CrashUploadListAndroid::BrowserCrashMetricsInitialized() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MinidumpUploadServiceImpl_browserCrashMetricsInitialized(env);
}

// static
bool CrashUploadListAndroid::DidBrowserCrashRecently() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MinidumpUploadServiceImpl_didBrowserCrashRecently(env);
}

std::vector<std::unique_ptr<UploadList::UploadInfo>>
CrashUploadListAndroid::LoadUploadList() {
  std::vector<std::unique_ptr<UploadInfo>> uploads;
  LoadUnsuccessfulUploadList(&uploads);

  auto complete_uploads = TextLogUploadList::LoadUploadList();
  for (auto& info : complete_uploads) {
    uploads.push_back(std::move(info));
  }
  return uploads;
}

void CrashUploadListAndroid::RequestSingleUpload(const std::string& local_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_local_id =
      base::android::ConvertUTF8ToJavaString(env, local_id);
  Java_MinidumpUploadServiceImpl_tryUploadCrashDumpWithLocalId(env, j_local_id);
}

void CrashUploadListAndroid::LoadUnsuccessfulUploadList(
    std::vector<std::unique_ptr<UploadInfo>>* uploads) {
  const char pending_uploads[] = ".dmp";
  const char skipped_uploads[] = ".skipped";
  const char manually_forced_uploads[] = ".forced";

  base::FileEnumerator files(upload_log_path().DirName(), false,
                             base::FileEnumerator::FILES);
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    UploadList::UploadInfo::State upload_state;
    if (file.value().find(manually_forced_uploads) != std::string::npos) {
      RecordUnsuccessfulUploadListState(UnsuccessfulUploadListState::FORCED);
      upload_state = UploadList::UploadInfo::State::Pending_UserRequested;
    } else if (file.value().find(pending_uploads) != std::string::npos) {
      RecordUnsuccessfulUploadListState(UnsuccessfulUploadListState::PENDING);
      upload_state = UploadList::UploadInfo::State::Pending;
    } else if (file.value().find(skipped_uploads) != std::string::npos) {
      RecordUnsuccessfulUploadListState(
          UnsuccessfulUploadListState::NOT_UPLOADED);
      upload_state = UploadList::UploadInfo::State::NotUploaded;
    } else {
      // The |file| is something other than a minidump file, e.g. a logcat file.
      RecordUnsuccessfulUploadListState(
          UnsuccessfulUploadListState::OTHER_FILENAME_SUFFIX);
      continue;
    }

    base::File::Info info;
    if (!base::GetFileInfo(file, &info)) {
      RecordUnsuccessfulUploadListState(
          UnsuccessfulUploadListState::FAILED_TO_LOAD_FILE_INFO);
      continue;
    }

    int64_t file_size = 0;
    if (!base::GetFileSize(file, &file_size)) {
      RecordUnsuccessfulUploadListState(
          UnsuccessfulUploadListState::FAILED_TO_LOAD_FILE_SIZE);
      continue;
    }

    // Crash reports can have multiple extensions (e.g. foo.dmp, foo.dmp.try1,
    // foo.skipped.try0).
    file = file.BaseName();
    while (file != file.RemoveExtension())
      file = file.RemoveExtension();

    // ID is the last part of the file name. e.g.
    // chromium-renderer-minidump-f297dbcba7a2d0bb.
    std::string id = file.value();
    std::size_t pos = id.find_last_of("-");
    if (pos == std::string::npos) {
      RecordUnsuccessfulUploadListState(
          UnsuccessfulUploadListState::FAILED_TO_FIND_DASH);
      continue;
    }

    RecordUnsuccessfulUploadListState(
        UnsuccessfulUploadListState::ADDING_AN_UPLOAD_ENTRY);
    id = id.substr(pos + 1);
    uploads->push_back(std::make_unique<UploadList::UploadInfo>(
        id, info.creation_time, upload_state, file_size));
  }
}

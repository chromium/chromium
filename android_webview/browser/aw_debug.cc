// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>

#include "android_webview/browser_jni_headers/AwDebug_jni.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"
#include "components/minidump_uploader/rewrite_minidumps_as_mimes.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/util/net/http_body.h"
#include "third_party/crashpad/crashpad/util/net/http_multipart_builder.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

class AwDebugCrashReporterClient
    : public ::crash_reporter::CrashReporterClient {
 public:
  AwDebugCrashReporterClient() = default;
  ~AwDebugCrashReporterClient() override = default;

  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override {
    *product_name = "AndroidWebView";
    *version = PRODUCT_VERSION;
    *channel =
        version_info::GetChannelString(version_info::android::GetChannel());
  }

  bool GetCrashDumpLocation(base::FilePath* debug_dir) override {
    base::FilePath cache_dir;
    if (!base::android::GetCacheDirectory(&cache_dir)) {
      return false;
    }
    *debug_dir = cache_dir.Append(FILE_PATH_LITERAL("WebView")).Append("Debug");
    return true;
  }

  void GetSanitizationInformation(const char* const** annotations_whitelist,
                                  void** target_module,
                                  bool* sanitize_stacks) override {
    *annotations_whitelist = crash_keys::kWebViewCrashKeyWhiteList;
    *target_module = nullptr;
    *sanitize_stacks = true;
  }

  DISALLOW_COPY_AND_ASSIGN(AwDebugCrashReporterClient);
};

// Writes the most recent report in the database as a MIME to fd. Finishes by
// deleting all reports from the database. Returns `true` if a report was
// successfully found and written the file descriptor.
bool WriteLastReportToFd(crashpad::CrashReportDatabase* db, int fd) {
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  if (db->GetPendingReports(&reports) !=
          crashpad::CrashReportDatabase::kNoError ||
      !reports.size()) {
    LOG(ERROR) << "no reports";
    return false;
  }

  size_t most_recent_index = 0;
  time_t most_recent_time = reports[0].creation_time;
  for (size_t index = 1; index < reports.size(); ++index) {
    if (reports[index].creation_time > most_recent_time) {
      most_recent_index = index;
      most_recent_time = reports[index].creation_time;
    }
  }

  {
    std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
        upload_report;
    if (db->GetReportForUploading(reports[most_recent_index].uuid,
                                  &upload_report,
                                  /* report_metrics= */ false) !=
        crashpad::CrashReportDatabase::kNoError) {
      return false;
    }

    crashpad::HTTPMultipartBuilder builder;
    pid_t pid;
    if (!minidump_uploader::MimeifyReport(*upload_report.get(), &builder,
                                          &pid)) {
      return false;
    }

    crashpad::WeakFileHandleFileWriter writer(fd);
    if (!minidump_uploader::WriteBodyToFile(builder.GetBodyStream().get(),
                                            &writer)) {
      return false;
    }
  }

  for (size_t index = 0; index < reports.size(); ++index) {
    db->DeleteReport(reports[index].uuid);
  }
  db->CleanDatabase(0);

  return true;
}

}  // namespace

static jboolean JNI_AwDebug_DumpWithoutCrashing(
    JNIEnv* env,
    const JavaParamRef<jstring>& dump_path) {
  // This may be called from any thread, and we might be in a state
  // where it is impossible to post tasks, so we have to be prepared
  // to do IO from this thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::File target(base::FilePath(ConvertJavaStringToUTF8(env, dump_path)),
                    base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
  if (!target.IsValid())
    return false;

  if (!CrashReporterEnabled()) {
    static constexpr char kMessage[] = "WebView isn't initialized";
    return static_cast<size_t>(target.WriteAtCurrentPos(
               kMessage, strlen(kMessage))) == strlen(kMessage);
  }

  AwDebugCrashReporterClient client;
  base::FilePath database_path;
  if (!client.GetCrashDumpLocation(&database_path)) {
    return false;
  }

  if (!base::CreateDirectory(database_path)) {
    return false;
  }

  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::Initialize(database_path);
  if (!database) {
    return false;
  }

  if (!::crash_reporter::DumpWithoutCrashingForClient(&client)) {
    return false;
  }

  return WriteLastReportToFd(database.get(), target.GetPlatformFile());
}

static void JNI_AwDebug_InitCrashKeysForWebViewTesting(JNIEnv* env) {
  crash_keys::InitCrashKeysForWebViewTesting();
}

static void JNI_AwDebug_SetWhiteListedKeyForTesting(JNIEnv* env) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      "AW_WHITELISTED_DEBUG_KEY");
  crash_key.Set("AW_DEBUG_VALUE");
}

static void JNI_AwDebug_SetNonWhiteListedKeyForTesting(JNIEnv* env) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      "AW_NONWHITELISTED_DEBUG_KEY");
  crash_key.Set("AW_DEBUG_VALUE");
}

static void JNI_AwDebug_SetSupportLibraryWebkitVersionCrashKey(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& version) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      crash_keys::kSupportLibraryWebkitVersion);
  crash_key.Set(ConvertJavaStringToUTF8(env, version));
}

}  // namespace android_webview

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace bookmarks {
class BookmarkModel;
}

namespace history_report {

class DataObserver;
class DataProvider;
class DeltaFileService;
class UsageReportsBufferService;

class HistoryReportJniBridge {
 public:
  HistoryReportJniBridge(JNIEnv* env, jobject obj);

  HistoryReportJniBridge(const HistoryReportJniBridge&) = delete;
  HistoryReportJniBridge& operator=(const HistoryReportJniBridge&) = delete;

  ~HistoryReportJniBridge();

  // Removes entries with seqno <= seq_no_lower_bound from delta file.
  // Returns biggest seqno in delta file.
  jlong TrimDeltaFile(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong seq_no_lower_bound);
  // Queries delta file for up to limit entries with seqno > last_seq_no.
  base::android::ScopedJavaLocalRef<jobjectArray> Query(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong last_seq_no,
      jint limit);
  // Queries usage reports buffer for a batch of reports to be reported for
  // local indexing.
  base::android::ScopedJavaLocalRef<jobjectArray> GetUsageReportsBatch(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint batch_size);
  // Removes a batch of usage reports from a usage reports buffer.
  void RemoveUsageReports(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& batch);
  // Clear all entries from the usage reports.
  void ClearUsageReports(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  // Populates the usage reports buffer with historic visits.
  // This should happen only once per corpus registration.
  jboolean AddHistoricVisitsToUsageReportsBuffer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  // Dumps internal state.
  base::android::ScopedJavaLocalRef<jstring> Dump(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void NotifyDataChanged();
  void NotifyDataCleared();
  void StopReporting();
  void StartReporting();

 private:
  JavaObjectWeakGlobalRef weak_java_provider_;
  std::unique_ptr<DataObserver> data_observer_;
  std::unique_ptr<DataProvider> data_provider_;
  std::unique_ptr<DeltaFileService> delta_file_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<UsageReportsBufferService> usage_reports_buffer_service_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_

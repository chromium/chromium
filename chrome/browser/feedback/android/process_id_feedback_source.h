// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_ANDROID_PROCESS_ID_FEEDBACK_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_ANDROID_PROCESS_ID_FEEDBACK_SOURCE_H_

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"

namespace chrome {
namespace android {

// Native class for Java counterpart. List up child process ID's by their type.
// Destroys itself when process id map building is completed, which is initiated
// by ctor.
class ProcessIdFeedbackSource
    : public base::RefCountedThreadSafe<ProcessIdFeedbackSource> {
 public:
  ProcessIdFeedbackSource(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  ProcessIdFeedbackSource(const ProcessIdFeedbackSource&) = delete;
  ProcessIdFeedbackSource& operator=(const ProcessIdFeedbackSource&) = delete;

  base::android::ScopedJavaLocalRef<jlongArray> GetProcessIdsForType(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint process_type);
  void PrepareProcessIds();

 private:
  friend base::RefCountedThreadSafe<ProcessIdFeedbackSource>;
  ~ProcessIdFeedbackSource();

  std::map<int, std::vector<base::ProcessHandle>> process_ids_;
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_ANDROID_PROCESS_ID_FEEDBACK_SOURCE_H_

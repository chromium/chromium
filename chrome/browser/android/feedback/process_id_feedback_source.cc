// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feedback/process_id_feedback_source.h"

#include "base/task/post_task.h"
#include "chrome/android/chrome_jni_headers/ProcessIdFeedbackSource_jni.h"
#include "content/public/browser/browser_task_traits.h"

#include "base/android/jni_array.h"
#include "base/bind.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace chrome {
namespace android {

int64_t JNI_ProcessIdFeedbackSource_GetCurrentPid(JNIEnv* env) {
  return base::GetCurrentProcId();
}

void JNI_ProcessIdFeedbackSource_Start(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  scoped_refptr<ProcessIdFeedbackSource> source =
      new ProcessIdFeedbackSource(env, obj);
  source->PrepareProcessIds();
}

ProcessIdFeedbackSource::ProcessIdFeedbackSource(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : java_ref_(env, obj) {}

ProcessIdFeedbackSource::~ProcessIdFeedbackSource() {}

void ProcessIdFeedbackSource::PrepareProcessIds() {
  // Browser child process info needs accessing on IO thread, while renderer
  // process info on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* host = it.GetCurrentValue();
    process_ids_[content::PROCESS_TYPE_RENDERER].push_back(
        host->GetProcess().Pid());
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ProcessIdFeedbackSource::PrepareProcessIdsOnIOThread,
                     this));
}

void ProcessIdFeedbackSource::PrepareProcessIdsOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter)
    process_ids_[iter.GetData().process_type].push_back(
        iter.GetData().GetProcess().Handle());

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ProcessIdFeedbackSource::PrepareCompleted, this));
}

void ProcessIdFeedbackSource::PrepareCompleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  DCHECK(!obj.is_null());
  Java_ProcessIdFeedbackSource_prepareCompleted(
      env, obj, reinterpret_cast<intptr_t>(this));
}

ScopedJavaLocalRef<jlongArray> ProcessIdFeedbackSource::GetProcessIdsForType(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_RENDERER:
    case content::PROCESS_TYPE_UTILITY:
    case content::PROCESS_TYPE_GPU:
      break;
    default:
      NOTREACHED() << "Unsupported process type.";
  }
  size_t size = process_ids_[process_type].size();

  jlong pids[size];
  for (size_t i = 0; i < size; i++)
    pids[i] = process_ids_[process_type][i];

  return base::android::ToJavaLongArray(env, pids, size);
}

}  // namespace android
}  // namespace chrome

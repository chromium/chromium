// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/net/download_file_task.h"

#include "android_webview/nonembedded/nonembedded_jni_headers/DownloadFileTask_jni.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

using TaskWeakPtr = base::WeakPtr<DownloadFileTask>;

void InvokeDownload(TaskWeakPtr weak_ptr,
                    scoped_refptr<base::SequencedTaskRunner> task_runner,
                    const GURL& url,
                    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadFileTask_download(
      env, reinterpret_cast<intptr_t>(&weak_ptr),
      reinterpret_cast<intptr_t>(task_runner.get()),
      url::GURLAndroid::FromNativeGURL(env, url),
      base::android::ConvertUTF8ToJavaString(env, file_path.value()));
}

}  // namespace

// static
void JNI_DownloadFileTask_CallProgressCallback(JNIEnv* env,
                                               jlong weak_ptr,
                                               jlong task_runner,
                                               jlong current) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  (native_task_runner)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&DownloadFileTask::InvokeProgressCallback,
                                *task, current));
}

// static
void JNI_DownloadFileTask_CallResponseStartedCallback(JNIEnv* env,
                                                      jlong weak_ptr,
                                                      jlong task_runner,
                                                      jint response_code,
                                                      jlong content_length) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  (native_task_runner)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&DownloadFileTask::InvokeResponseStartedCallback,
                         *task, response_code, content_length));
}

// static
void JNI_DownloadFileTask_CallDownloadToFileCompleteCallback(
    JNIEnv* env,
    jlong weak_ptr,
    jlong task_runner,
    jint network_error,
    jlong content_size) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  (native_task_runner)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &DownloadFileTask::InvokeDownloadToFileCompleteCallback,
                     *task, network_error, content_size));
}

DownloadFileTask::DownloadFileTask(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback)
    : NetworkTask(std::move(response_started_callback),
                  std::move(progress_callback)),
      download_to_file_complete_callback_(
          std::move(download_to_file_complete_callback)) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&InvokeDownload, weak_ptr_factory_.GetWeakPtr(),
                     task_runner_, url, file_path));
}

DownloadFileTask::~DownloadFileTask() = default;

void DownloadFileTask::InvokeProgressCallback(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(current);
}

void DownloadFileTask::InvokeResponseStartedCallback(int response_code,
                                                     int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(response_started_callback_).Run(response_code, content_length);
}

void DownloadFileTask::InvokeDownloadToFileCompleteCallback(
    int network_error,
    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(download_to_file_complete_callback_)
      .Run(network_error, content_size);
}

}  // namespace android_webview
// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/net/network_fetcher_task.h"

#include <string>
#include <utility>
#include <vector>

#include "android_webview/nonembedded/net/network_impl.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/nonembedded/nonembedded_jni_headers/NetworkFetcherTask_jni.h"

namespace android_webview {

namespace {
using TaskWeakPtr = base::WeakPtr<NetworkFetcherTask>;

void InvokePostRequest(
    TaskWeakPtr weak_ptr,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  std::vector<std::string> keys, values;
  for (auto const& header : post_additional_headers) {
    keys.push_back(header.first);
    values.push_back(header.second);
  }

  Java_NetworkFetcherTask_postRequest(
      env, reinterpret_cast<intptr_t>(&weak_ptr),
      reinterpret_cast<intptr_t>(task_runner.get()),
      url::GURLAndroid::FromNativeGURL(env, url),
      base::android::ToJavaByteArray(env, post_data),
      base::android::ConvertUTF8ToJavaString(env, content_type),
      base::android::ToJavaArrayOfStrings(env, keys),
      base::android::ToJavaArrayOfStrings(env, values));
}

void InvokeDownload(TaskWeakPtr weak_ptr,
                    scoped_refptr<base::SequencedTaskRunner> task_runner,
                    const GURL& url,
                    const base::FilePath& file_path) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NetworkFetcherTask_download(
      env, reinterpret_cast<intptr_t>(&weak_ptr),
      reinterpret_cast<intptr_t>(task_runner.get()),
      url::GURLAndroid::FromNativeGURL(env, url),
      base::android::ConvertUTF8ToJavaString(env, file_path.value()));
}

}  // namespace

// static
void JNI_NetworkFetcherTask_CallProgressCallback(JNIEnv* env,
                                                 jlong weak_ptr,
                                                 jlong task_runner,
                                                 jlong current) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  native_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&NetworkFetcherTask::InvokeProgressCallback,
                                *task, current));
}

// static
void JNI_NetworkFetcherTask_CallResponseStartedCallback(JNIEnv* env,
                                                        jlong weak_ptr,
                                                        jlong task_runner,
                                                        jint response_code,
                                                        jlong content_length) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  native_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkFetcherTask::InvokeResponseStartedCallback, *task,
                     response_code, content_length));
}

// static
void JNI_NetworkFetcherTask_CallDownloadToFileCompleteCallback(
    JNIEnv* env,
    jlong weak_ptr,
    jlong task_runner,
    jint network_error,
    jlong content_size) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  native_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkFetcherTask::InvokeDownloadToFileCompleteCallback,
                     *task, network_error, content_size));
}

// static
void JNI_NetworkFetcherTask_CallPostRequestCompleteCallback(
    JNIEnv* env,
    jlong weak_ptr,
    jlong task_runner,
    const base::android::JavaParamRef<jbyteArray>& response_body,
    jint network_error,
    const base::android::JavaParamRef<jstring>& header_e_tag,
    const base::android::JavaParamRef<jstring>& header_x_cup_server_proof,
    jlong x_header_retry_after_sec) {
  auto* native_task_runner =
      reinterpret_cast<base::SequencedTaskRunner*>(task_runner);
  DCHECK(native_task_runner);

  auto* task = reinterpret_cast<TaskWeakPtr*>(weak_ptr);
  std::string response_body_str;
  base::android::JavaByteArrayToString(env, response_body, &response_body_str);
  native_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkFetcherTask::InvokePostRequestCompleteCallback,
                     *task, std::make_unique<std::string>(response_body_str),
                     network_error,
                     base::android::ConvertJavaStringToUTF8(env, header_e_tag),
                     base::android::ConvertJavaStringToUTF8(
                         env, header_x_cup_server_proof),
                     x_header_retry_after_sec));
}

// static
std::unique_ptr<NetworkFetcherTask>
NetworkFetcherTask::CreateDownloadToFileTask(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback) {
  return std::make_unique<NetworkFetcherTask>(
      url, file_path, std::move(response_started_callback), progress_callback,
      std::move(download_to_file_complete_callback));
}

// static
std::unique_ptr<NetworkFetcherTask> NetworkFetcherTask::CreatePostRequestTask(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::PostRequestCompleteCallback
        post_request_complete_callback) {
  return std::make_unique<NetworkFetcherTask>(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), progress_callback,
      std::move(post_request_complete_callback));
}

NetworkFetcherTask::NetworkFetcherTask(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback)
    : response_started_callback_(std::move(response_started_callback)),
      progress_callback_(std::move(progress_callback)),
      download_to_file_complete_callback_(
          std::move(download_to_file_complete_callback)) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&InvokeDownload, weak_ptr_factory_.GetWeakPtr(),
                     task_runner_, url, file_path));
}

NetworkFetcherTask::NetworkFetcherTask(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::PostRequestCompleteCallback
        post_request_complete_callback)
    : response_started_callback_(std::move(response_started_callback)),
      progress_callback_(std::move(progress_callback)),
      post_request_complete_callback_(
          std::move(post_request_complete_callback)) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&InvokePostRequest, weak_ptr_factory_.GetWeakPtr(),
                     task_runner_, url, post_data, content_type,
                     post_additional_headers));
}

NetworkFetcherTask::~NetworkFetcherTask() = default;

void NetworkFetcherTask::InvokeProgressCallback(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  progress_callback_.Run(current);
}

void NetworkFetcherTask::InvokeResponseStartedCallback(int response_code,
                                                       int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(response_started_callback_).Run(response_code, content_length);
}

void NetworkFetcherTask::InvokeDownloadToFileCompleteCallback(
    int network_error,
    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(download_to_file_complete_callback_)
      .Run(network_error, content_size);
}

void NetworkFetcherTask::InvokePostRequestCompleteCallback(
    std::unique_ptr<std::string> response_body,
    int network_error,
    const std::string& header_etag,
    const std::string& header_x_cup_server_proof,
    int64_t x_header_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(post_request_complete_callback_)
      .Run(std::move(response_body), network_error, header_etag,
           header_x_cup_server_proof, x_header_retry_after_sec);
}

}  // namespace android_webview

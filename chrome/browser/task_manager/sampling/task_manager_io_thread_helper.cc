// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_manager_io_thread_helper.h"

#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/task_manager/sampling/task_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

namespace {

TaskManagerIoThreadHelper* g_io_thread_helper = nullptr;

}  // namespace

size_t BytesTransferredKey::Hasher::operator()(
    const BytesTransferredKey& key) const {
  return base::HashInts(key.child_id, key.route_id);
}

bool BytesTransferredKey::operator==(const BytesTransferredKey& other) const {
  return child_id == other.child_id && route_id == other.route_id;
}

IoThreadHelperManager::IoThreadHelperManager(
    BytesTransferredCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&TaskManagerIoThreadHelper::CreateInstance,
                 std::move(result_callback)));
}

IoThreadHelperManager::~IoThreadHelperManager() {
  // This may be called at exit time when the main thread is no longer
  // registered as the UI thread.
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&TaskManagerIoThreadHelper::DeleteInstance));
}

// static
void TaskManagerIoThreadHelper::CreateInstance(
    BytesTransferredCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!g_io_thread_helper);

  g_io_thread_helper =
      new TaskManagerIoThreadHelper(std::move(result_callback));
}

// static
void TaskManagerIoThreadHelper::DeleteInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  delete g_io_thread_helper;
  g_io_thread_helper = nullptr;
}

// static
void TaskManagerIoThreadHelper::OnRawBytesTransferred(BytesTransferredKey key,
                                                      int64_t bytes_read,
                                                      int64_t bytes_sent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (g_io_thread_helper)
    g_io_thread_helper->OnNetworkBytesTransferred(key, bytes_read, bytes_sent);
}

TaskManagerIoThreadHelper::TaskManagerIoThreadHelper(
    BytesTransferredCallback result_callback)
    : result_callback_(std::move(result_callback)), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

TaskManagerIoThreadHelper::~TaskManagerIoThreadHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void TaskManagerIoThreadHelper::OnMultipleBytesTransferredIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!bytes_transferred_unordered_map_.empty());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(result_callback_,
                 std::move(bytes_transferred_unordered_map_)));
  bytes_transferred_unordered_map_.clear();
  DCHECK(bytes_transferred_unordered_map_.empty());
}

void TaskManagerIoThreadHelper::OnNetworkBytesTransferred(
    BytesTransferredKey key,
    int64_t bytes_read,
    int64_t bytes_sent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (bytes_transferred_unordered_map_.empty()) {
    // Schedule a task to process the transferred bytes requests a second from
    // now. We're trying to calculate the tasks' network usage speed as bytes
    // per second so we collect as many requests during one seconds before the
    // below delayed TaskManagerIoThreadHelper::OnMultipleBytesReadIO() process
    // them after one second from now.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TaskManagerIoThreadHelper::OnMultipleBytesTransferredIO,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
  }

  BytesTransferredParam& entry = bytes_transferred_unordered_map_[key];
  entry.byte_read_count += bytes_read;
  entry.byte_sent_count += bytes_sent;
}

}  // namespace task_manager

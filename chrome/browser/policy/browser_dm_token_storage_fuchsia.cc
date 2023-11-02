// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_fuchsia.h"

#include <string>

#include "base/notreached.h"
#include "base/task/thread_pool.h"

namespace policy {
namespace {
bool LogAndDoNothing() {
  // TODO(crbug.com/1236996)
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}
}  // namespace

BrowserDMTokenStorageFuchsia::BrowserDMTokenStorageFuchsia()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageFuchsia::~BrowserDMTokenStorageFuchsia() {}

std::string BrowserDMTokenStorageFuchsia::InitClientId() {
  // TODO(crbug.com/1236996)
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string();
}

std::string BrowserDMTokenStorageFuchsia::InitEnrollmentToken() {
  // TODO(crbug.com/1236996)
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string();
}

std::string BrowserDMTokenStorageFuchsia::InitDMToken() {
  // TODO(crbug.com/1236996)
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string();
}

bool BrowserDMTokenStorageFuchsia::InitEnrollmentErrorOption() {
  // TODO(crbug.com/1236996)
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageFuchsia::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  // TODO(crbug.com/1236996)
  return base::BindOnce(&LogAndDoNothing);
}

BrowserDMTokenStorage::StoreTask
BrowserDMTokenStorageFuchsia::DeleteDMTokenTask(const std::string& client_id) {
  // TODO(crbug.com/1236996)
  return base::BindOnce(&LogAndDoNothing);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageFuchsia::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy

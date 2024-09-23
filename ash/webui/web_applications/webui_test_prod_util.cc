// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/webui_test_prod_util.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"

using content::WebUIDataSource;

namespace {

WebUIDataSource::ShouldHandleRequestCallback& GetTestShouldHandleRequest() {
  static base::NoDestructor<WebUIDataSource::ShouldHandleRequestCallback>
      callback;
  return *callback;
}

WebUIDataSource::HandleRequestCallback& GetTestRequestFilterHandler() {
  static base::NoDestructor<WebUIDataSource::HandleRequestCallback> callback;
  return *callback;
}

bool InvokeTestShouldHandleRequestCallback(
    const WebUIDataSource::ShouldHandleRequestCallback&
        real_should_handle_request_callback,
    const std::string& path) {
  const auto& test_callback = GetTestShouldHandleRequest();
  if (test_callback && test_callback.Run(path)) {
    return true;
  }
  return real_should_handle_request_callback &&
         real_should_handle_request_callback.Run(path);
}

void InvokeTestFileRequestFilterCallback(
    const WebUIDataSource::HandleRequestCallback& real_handle_request_callback,
    const std::string& path,
    WebUIDataSource::GotDataCallback callback) {
  // First, check whether this was request for a test-only resource. This
  // requires the test handler to be installed by
  // SetTestableDataSourceRequestHandlerForTesting() and for it to have returned
  // true. Otherwise assume, that since the request was directed to the filter,
  // that it is a request for the "real" filter installed by
  // MaybeConfigureTestableDataSource().
  const auto& test_callback = GetTestShouldHandleRequest();
  if (test_callback && test_callback.Run(path)) {
    GetTestRequestFilterHandler().Run(path, std::move(callback));
  } else {
    DCHECK(!real_handle_request_callback.is_null());
    real_handle_request_callback.Run(path, std::move(callback));
  }
}

// Determines whether, when attempting to load a path, we want to, instead of
// using the regular handler, load it from a file on disk.
bool ShouldLoadResponseFromDisk(const base::FilePath& root,
                                const std::string& path) {
  const base::FilePath expanded = root.Append(path);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool exists = base::PathExists(expanded);
  if (exists) {
    VLOG(1) << "Loading test data from " << expanded << " for " << path;
  } else {
    VLOG(1) << "Unable to load test data from " << expanded << " for " << path
            << ", as the file doesn't exist.";
  }
  return exists;
}

void LoadFileFromDisk(const base::FilePath& path,
                      content::WebUIDataSource::GotDataCallback callback) {
  std::string result;
  CHECK(base::ReadFileToString(path, &result));

  std::move(callback).Run(
      new base::RefCountedBytes(base::as_byte_span(result)));
}

void LoadResponseFromDisk(const base::FilePath& root,
                          const std::string& path,
                          content::WebUIDataSource::GotDataCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(LoadFileFromDisk, root.Append(path), std::move(callback)));
}

}  // namespace

bool MaybeConfigureTestableDataSource(
    WebUIDataSource* host_source,
    const std::string& handler_name,
    const WebUIDataSource::ShouldHandleRequestCallback&
        real_should_handle_request_callback,
    const WebUIDataSource::HandleRequestCallback&
        real_handle_request_callback) {
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  bool has_test_handler = false;
  if (cmd.HasSwitch(::ash::switches::kWebUiDataSourcePathForTesting) &&
      !handler_name.empty()) {
    const base::FilePath root =
        cmd.GetSwitchValuePath(::ash::switches::kWebUiDataSourcePathForTesting)
            .Append(handler_name);
    GetTestShouldHandleRequest() =
        base::BindRepeating(ShouldLoadResponseFromDisk, root);
    GetTestRequestFilterHandler() =
        base::BindRepeating(LoadResponseFromDisk, root);
    has_test_handler = true;
  } else if (cmd.HasSwitch(::switches::kTestType)) {
    has_test_handler = true;
  }

  if (has_test_handler) {
    host_source->SetRequestFilter(
        base::BindRepeating(&InvokeTestShouldHandleRequestCallback,
                            real_should_handle_request_callback),
        base::BindRepeating(&InvokeTestFileRequestFilterCallback,
                            real_handle_request_callback));
  } else {
    host_source->SetRequestFilter(real_should_handle_request_callback,
                                  real_handle_request_callback);
  }
  return has_test_handler;
}

void SetTestableDataSourceRequestHandlerForTesting(  // IN-TEST
    WebUIDataSource::ShouldHandleRequestCallback should_handle,
    WebUIDataSource::HandleRequestCallback handler) {
  GetTestShouldHandleRequest() = std::move(should_handle);
  GetTestRequestFilterHandler() = std::move(handler);
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/webui_test_prod_util.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
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

}  // namespace

bool MaybeConfigureTestableDataSource(
    WebUIDataSource* host_source,
    const WebUIDataSource::ShouldHandleRequestCallback&
        real_should_handle_request_callback,
    const WebUIDataSource::HandleRequestCallback&
        real_handle_request_callback) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    host_source->SetRequestFilter(real_should_handle_request_callback,
                                  real_handle_request_callback);
    return false;
  }

  host_source->SetRequestFilter(
      base::BindRepeating(&InvokeTestShouldHandleRequestCallback,
                          real_should_handle_request_callback),
      base::BindRepeating(&InvokeTestFileRequestFilterCallback,
                          real_handle_request_callback));
  return true;
}

void SetTestableDataSourceRequestHandlerForTesting(  // IN-TEST
    WebUIDataSource::ShouldHandleRequestCallback should_handle,
    WebUIDataSource::HandleRequestCallback handler) {
  GetTestShouldHandleRequest() = std::move(should_handle);
  GetTestRequestFilterHandler() = std::move(handler);
}

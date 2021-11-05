// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/webui_test_prod_util.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "content/public/common/content_switches.h"

namespace {

content::WebUIDataSource::ShouldHandleRequestCallback&
GetTestShouldHandleRequest() {
  static base::NoDestructor<
      content::WebUIDataSource::ShouldHandleRequestCallback>
      callback;
  return *callback;
}

content::WebUIDataSource::HandleRequestCallback& GetTestRequestFilterHandler() {
  static base::NoDestructor<content::WebUIDataSource::HandleRequestCallback>
      callback;
  return *callback;
}

bool InvokeTestShouldHandleRequestCallback(const std::string& path) {
  const auto& callback = GetTestShouldHandleRequest();
  return callback ? callback.Run(path) : false;
}

void InvokeTestFileRequestFilterCallback(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  return GetTestRequestFilterHandler().Run(path, std::move(callback));
}

}  // namespace

bool MaybeConfigureTestableDataSource(content::WebUIDataSource* host_source) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType))
    return false;

  host_source->SetRequestFilter(
      base::BindRepeating(&InvokeTestShouldHandleRequestCallback),
      base::BindRepeating(&InvokeTestFileRequestFilterCallback));
  return true;
}

void SetTestableDataSourceRequestHandlerForTesting(  // IN-TEST
    content::WebUIDataSource::ShouldHandleRequestCallback should_handle,
    content::WebUIDataSource::HandleRequestCallback handler) {
  GetTestShouldHandleRequest() = std::move(should_handle);
  GetTestRequestFilterHandler() = std::move(handler);
}

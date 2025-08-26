// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DISPATCH_HTTP_REQUEST_PARAMS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DISPATCH_HTTP_REQUEST_PARAMS_H_

#include <optional>
#include <string>

#include "base/values.h"

struct DevToolsDispatchHttpRequestParams {
  DevToolsDispatchHttpRequestParams();
  ~DevToolsDispatchHttpRequestParams();
  DevToolsDispatchHttpRequestParams(const DevToolsDispatchHttpRequestParams&);
  DevToolsDispatchHttpRequestParams& operator=(
      const DevToolsDispatchHttpRequestParams&);
  DevToolsDispatchHttpRequestParams(DevToolsDispatchHttpRequestParams&&);
  DevToolsDispatchHttpRequestParams& operator=(
      DevToolsDispatchHttpRequestParams&&);

  static std::optional<DevToolsDispatchHttpRequestParams> FromDict(
      const base::Value::Dict& dict);

  std::string service;
  std::string path;
  std::string method;
  std::optional<std::string> body;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DISPATCH_HTTP_REQUEST_PARAMS_H_

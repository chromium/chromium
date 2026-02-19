// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"

DevToolsDispatchHttpRequestParams::DevToolsDispatchHttpRequestParams() =
    default;
DevToolsDispatchHttpRequestParams::~DevToolsDispatchHttpRequestParams() =
    default;
DevToolsDispatchHttpRequestParams::DevToolsDispatchHttpRequestParams(
    const DevToolsDispatchHttpRequestParams&) = default;
DevToolsDispatchHttpRequestParams& DevToolsDispatchHttpRequestParams::operator=(
    const DevToolsDispatchHttpRequestParams&) = default;
DevToolsDispatchHttpRequestParams::DevToolsDispatchHttpRequestParams(
    DevToolsDispatchHttpRequestParams&&) = default;
DevToolsDispatchHttpRequestParams& DevToolsDispatchHttpRequestParams::operator=(
    DevToolsDispatchHttpRequestParams&&) = default;

// static
std::optional<DevToolsDispatchHttpRequestParams>
DevToolsDispatchHttpRequestParams::FromDict(const base::DictValue& dict) {
  const std::string* service = dict.FindString("service");
  if (!service) {
    return std::nullopt;
  }

  const std::string* path = dict.FindString("path");
  if (!path) {
    return std::nullopt;
  }

  const std::string* method = dict.FindString("method");
  if (!method) {
    return std::nullopt;
  }

  DevToolsDispatchHttpRequestParams params;
  params.service = *service;
  params.path = *path;
  params.method = *method;
  const std::string* body = dict.FindString("body");
  if (body) {
    params.body = *body;
  }

  const base::DictValue* query_params_dict = dict.FindDict("queryParams");

  if (query_params_dict) {
    for (auto [key, value] : *query_params_dict) {
      if (const std::string* str = value.GetIfString()) {
        params.query_params[key].push_back(*str);
      } else if (const base::ListValue* list = value.GetIfList()) {
        for (const auto& item : *list) {
          if (const std::string* item_str = item.GetIfString()) {
            params.query_params[key].push_back(*item_str);
          }
        }
      }
    }
  }

  params.stream_id = dict.FindInt("streamId");
  return params;
}

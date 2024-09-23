// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_
#define CHROME_BROWSER_HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_

#include <string>
#include <string_view>

#include "base/values.h"

namespace simple_devtools_protocol_client {
class SimpleDevToolsProtocolClient;
}

namespace headless {

// TODO(kvitekp): Move these helpers to
// //components/simple_devtools_protocol_client?

// Send DevTools command and wait for response by running a local
// message loop. This is typically used as a quick and dirty way
// to enable a domain.
base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command);
base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command,
    base::Value::Dict params);

// Convenience function to create a single key/value Dict.
template <typename T>
base::Value::Dict Param(std::string_view key, T&& value) {
  base::Value::Dict param;
  param.Set(key, std::move(value));
  return param;
}

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idltest/idltest_api.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/values.h"

namespace {

base::Value CopyBinaryValueToIntegerList(
    const base::Value::BlobStorage& input) {
  base::Value::List list;
  list.reserve(input.size());
  for (int c : input)
    list.Append(c);
  return base::Value(std::move(list));
}

}  // namespace

ExtensionFunction::ResponseAction IdltestSendArrayBufferFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(has_args() && !args().empty());
  const auto& value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(value.is_blob());
  return RespondNow(
      WithArguments(CopyBinaryValueToIntegerList(value.GetBlob())));
}

ExtensionFunction::ResponseAction IdltestSendArrayBufferViewFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(has_args() && !args().empty());
  const auto& value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(value.is_blob());
  return RespondNow(
      WithArguments(CopyBinaryValueToIntegerList(value.GetBlob())));
}

ExtensionFunction::ResponseAction IdltestGetArrayBufferFunction::Run() {
  static constexpr std::string_view kHello = "hello world";
  return RespondNow(
      WithArguments(base::Value(base::as_bytes(base::make_span(kHello)))));
}

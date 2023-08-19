// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FILES_INTERNALS_DEBUG_JSON_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FILES_INTERNALS_DEBUG_JSON_PROVIDER_H_

#include <string_view>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/values.h"

namespace ash {

class FilesInternalsDebugJSONProvider {
 public:
  virtual ~FilesInternalsDebugJSONProvider() = default;

  using JSONKeyValuePair = std::pair<std::string_view, base::Value>;

  // Gets human-readable debugging information as a JSON value. The key is
  // passed through unchanged to the callback.
  virtual void GetDebugJSONForKey(
      std::string_view key,
      base::OnceCallback<void(JSONKeyValuePair)> callback) = 0;

  // The function-pointer type of a free-standing (no implicit "this" argument)
  // equivalent of the GetDebugJSONForKey method.
  using FunctionPointerType =
      void (*)(std::string_view key,
               base::OnceCallback<void(JSONKeyValuePair)> callback);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FILES_INTERNALS_DEBUG_JSON_PROVIDER_H_

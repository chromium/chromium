// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/files_internals_ui_delegate.h"

#include "base/values.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"

ChromeFilesInternalsUIDelegate::ChromeFilesInternalsUIDelegate() = default;

base::Value ChromeFilesInternalsUIDelegate::GetDebugJSON() const {
  base::Value::Dict dict;

  if (fusebox::Server* fusebox_server = fusebox::Server::GetInstance()) {
    dict.Set("fusebox", fusebox_server->GetDebugJSON());
  } else {
    dict.Set("fusebox", base::Value());
  }

  return base::Value(std::move(dict));
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads_internal/downloads_internal_api.h"

#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/common/extensions/api/downloads.h"
#include "chrome/common/extensions/api/downloads_internal.h"

namespace extensions {

DownloadsInternalDetermineFilenameFunction::
    DownloadsInternalDetermineFilenameFunction() {}

DownloadsInternalDetermineFilenameFunction::
    ~DownloadsInternalDetermineFilenameFunction() {}

typedef extensions::api::downloads_internal::DetermineFilename::Params
    DetermineFilenameParams;

ExtensionFunction::ResponseAction
DownloadsInternalDetermineFilenameFunction::Run() {
  std::optional<DetermineFilenameParams> params =
      DetermineFilenameParams::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
  const std::string& filename = args()[1].GetString();
  std::string error;
  bool result = ExtensionDownloadsEventRouter::DetermineFilename(
      browser_context(), include_incognito_information(), extension()->id(),
      params->download_id, base::FilePath::FromUTF8Unsafe(filename),
      extensions::api::downloads::ParseFilenameConflictAction(
          params->conflict_action),
      &error);
  return RespondNow(result ? NoArguments() : Error(error));
}

}  // namespace extensions

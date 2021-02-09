// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/files_extension_function.h"

#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chromeos/components/file_manager/url_constants.h"

namespace extensions {

FilesExtensionFunction::FilesExtensionFunction()
    : file_app_id_(file_manager::kFileManagerAppId),
      swa_url_(chromeos::file_manager::kChromeUIFileManagerURL) {}

FilesExtensionFunction::~FilesExtensionFunction() = default;

const std::string& FilesExtensionFunction::extension_id_or_file_app_id() const {
#if !defined(OFFICAL_BUILD)
  if (!extension_) {
    CHECK(is_swa_mode());
    return file_app_id();
  }
#endif
  return extension_id();
}

bool FilesExtensionFunction::is_swa_mode() const {
  return source_url().GetOrigin() == swa_url_ &&
         source_context_type() == Feature::WEBUI_CONTEXT;
}

}  // namespace extensions

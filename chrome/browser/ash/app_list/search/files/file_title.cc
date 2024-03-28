// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_title.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace app_list {
namespace {

std::string StripHostedFileExtensions(const std::string& filename) {
  static const base::NoDestructor<std::vector<std::string>> hosted_extensions(
      {".GDOC", ".GSHEET", ".GSLIDES", ".GDRAW", ".GTABLE", ".GLINK", ".GFORM",
       ".GMAPS", ".GSITE"});

  for (const auto& extension : *hosted_extensions) {
    if (base::EndsWith(filename, extension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return filename.substr(0, filename.size() - extension.size());
    }
  }

  return filename;
}

}  // namespace

std::u16string GetFileTitle(const base::FilePath& file_path) {
  return base::UTF8ToUTF16(
      StripHostedFileExtensions(file_path.BaseName().value()));
}

}  // namespace app_list

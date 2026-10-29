// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_title.h"

#include <array>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace app_list {
namespace {

std::string_view StripHostedFileExtensions(std::string_view filename) {
  static constexpr auto kHostedExtensions = std::to_array<std::string_view>(
      {".GDOC", ".GSHEET", ".GSLIDES", ".GDRAW", ".GTABLE", ".GLINK", ".GFORM",
       ".GMAPS", ".GSITE"});

  for (std::string_view extension : kHostedExtensions) {
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

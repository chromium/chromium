// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/string_util.h"

#include <string_view>

namespace app_list {

std::string NormalizeId(std::string_view id) {
  // No existing scheme names include the delimiter string "://".
  std::size_t delimiter_index = id.find("://");
  if (delimiter_index != std::string_view::npos) {
    id.remove_prefix(delimiter_index + 3);
  }
  if (!id.empty() && id.back() == '/') {
    id.remove_suffix(1);
  }
  return std::string(id);
}

std::string RemoveAppShortcutLabel(std::string_view id) {
  std::size_t delimiter_index = id.find_last_of('/');
  if (delimiter_index != std::string_view::npos) {
    id = id.substr(0, delimiter_index);
  }
  return std::string(id);
}

std::optional<std::string> GetDriveId(const GURL& url) {
  if (url.GetHost() != "docs.google.com") {
    return std::nullopt;
  }

  std::string path = url.GetPath();

  // Extract everything between /d/ and the next slash.
  // For example, /presentation/d/abcdefg/edit -> abcdefg.
  std::string kPrefix = "/d/";
  size_t id_start = path.find(kPrefix);
  if (id_start == std::string::npos)
    return std::nullopt;
  id_start += kPrefix.size();

  size_t id_end = path.find("/", id_start);
  return std::move(path).substr(id_start, id_end - id_start);
}

}  // namespace app_list

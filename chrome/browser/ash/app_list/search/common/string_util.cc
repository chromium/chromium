// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/string_util.h"

namespace app_list {

std::string NormalizeId(const std::string& id) {
  std::string result(id);
  // No existing scheme names include the delimiter string "://".
  std::size_t delimiter_index = result.find("://");
  if (delimiter_index != std::string::npos)
    result.erase(0, delimiter_index + 3);
  if (!result.empty() && result.back() == '/')
    result.pop_back();
  return result;
}

std::string RemoveAppShortcutLabel(const std::string& id) {
  std::string result(id);
  std::size_t delimiter_index = result.find_last_of('/');
  if (delimiter_index != std::string::npos)
    result.erase(delimiter_index);
  return result;
}

std::optional<std::string> GetDriveId(const GURL& url) {
  if (url.host() != "docs.google.com")
    return std::nullopt;

  std::string path = url.path();

  // Extract everything between /d/ and the next slash.
  // For example, /presentation/d/abcdefg/edit -> abcdefg.
  std::string kPrefix = "/d/";
  size_t id_start = path.find(kPrefix);
  if (id_start == std::string::npos)
    return std::nullopt;
  id_start += kPrefix.size();

  size_t id_end = path.find("/", id_start);
  return path.substr(id_start, id_end - id_start);
}

}  // namespace app_list

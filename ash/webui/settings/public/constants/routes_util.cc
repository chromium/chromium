// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes_util.h"

#include <string_view>

#include "ash/webui/settings/public/constants/routes.h"

namespace chromeos::settings {

namespace {

std::string_view RemoveQuery(std::string_view path) {
  std::string_view::size_type input_index = path.find('?');
  if (input_index != std::string_view::npos) {
    return path.substr(0, input_index);
  }
  return path;
}

}  // namespace

bool IsOSSettingsSubPage(std::string_view sub_page) {
  // Sub-pages may have query parameters, e.g. networkDetail?guid=123456.
  std::string_view sub_page_without_query = RemoveQuery(sub_page);

  for (const char* p : kPaths) {
    std::string_view path_without_query = RemoveQuery(p);
    if (sub_page_without_query == path_without_query) {
      return true;
    }
  }

  return false;
}

}  // namespace chromeos::settings

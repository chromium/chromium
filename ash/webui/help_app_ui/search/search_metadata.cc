// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_metadata.h"

namespace ash {
namespace help_app {

SearchMetadata::SearchMetadata() = default;

SearchMetadata::SearchMetadata(const std::u16string& title,
                               const std::u16string& main_category,
                               const std::string& url_path_with_parameters)
    : title(title),
      main_category(main_category),
      url_path_with_parameters(url_path_with_parameters) {}

SearchMetadata::~SearchMetadata() = default;

}  // namespace help_app
}  // namespace ash

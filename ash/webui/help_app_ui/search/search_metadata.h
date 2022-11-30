// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_METADATA_H_
#define ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_METADATA_H_

#include <string>

namespace ash {
namespace help_app {

// Represents the metadata for a potential search result. The metadata is the
// information required to display a search result, but isn't provided by the
// local search service in the search response.
struct SearchMetadata {
  SearchMetadata();
  SearchMetadata(const std::u16string& title,
                 const std::u16string& main_category,
                 const std::string& url_path_with_parameters);
  SearchMetadata(const SearchMetadata& other) = delete;
  SearchMetadata& operator=(const SearchMetadata& other) = delete;
  SearchMetadata(SearchMetadata&& other) = default;
  SearchMetadata& operator=(SearchMetadata&& other) = default;
  ~SearchMetadata();

  // Localized title.
  std::u16string title;

  // Localized main category.
  std::u16string main_category;

  // The URL path containing the relevant content, which may or may not contain
  // URL parameters. For example, if the help content is at
  // chrome://help-app/help/sub/3399763/id/1282338#install-user, then this field
  // would be "help/sub/3399763/id/1282338#install-user".
  std::string url_path_with_parameters;
};

}  // namespace help_app
}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_METADATA_H_

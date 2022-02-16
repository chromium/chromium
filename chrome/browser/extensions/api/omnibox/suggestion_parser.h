// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/common/extensions/api/omnibox.h"

namespace extensions {

// A helper struct to hold description and styles, as parsed from an XML
// string. In practice, this corresponds to omnibox::DefaultSuggestResult, but
// it will also be used to construct SuggestResults, so we disambiguate with a
// common struct.
struct DescriptionAndStyles {
  DescriptionAndStyles();
  ~DescriptionAndStyles();

  std::u16string description;
  std::vector<api::omnibox::MatchClassification> styles;
};

using DescriptionAndStylesCallback =
    base::OnceCallback<void(std::unique_ptr<DescriptionAndStyles>)>;

// Parses `str`, which is the suggestion string passed from the extension that
// potentially contains XML markup (e.g., the string may be
// "visit <url>https://example.com</url>"). This parses the string in an
// isolated process and asynchronously returns the description and styles via
// `callback`. On failure (e.g. due to invalid XML), invokes `callback` with
// null.
void ParseDescriptionAndStyles(base::StringPiece str,
                               DescriptionAndStylesCallback callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_

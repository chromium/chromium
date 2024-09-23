// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/common/extensions/api/omnibox.h"

namespace extensions {

// A helper struct to hold description and styles, as parsed from an XML
// string. In practice, this corresponds to omnibox::DefaultSuggestResult, but
// it will also be used to construct SuggestResults, so we disambiguate with a
// common struct.
struct DescriptionAndStyles {
  DescriptionAndStyles();
  DescriptionAndStyles(const DescriptionAndStyles&) = delete;
  DescriptionAndStyles(DescriptionAndStyles&&);
  DescriptionAndStyles& operator=(const DescriptionAndStyles&) = delete;
  DescriptionAndStyles& operator=(DescriptionAndStyles&&);
  ~DescriptionAndStyles();

  std::u16string description;
  std::vector<api::omnibox::MatchClassification> styles;
};

// The container for the result from parsing descriptions and styles.
struct DescriptionAndStylesResult {
  // Chromium's clang plugin about non-trivial structs needing out-of-line
  // constructors requires us to go a little ham here.
  DescriptionAndStylesResult();
  DescriptionAndStylesResult(const DescriptionAndStylesResult&) = delete;
  DescriptionAndStylesResult(DescriptionAndStylesResult&&);
  DescriptionAndStylesResult& operator=(const DescriptionAndStylesResult&) =
      delete;
  DescriptionAndStylesResult& operator=(DescriptionAndStylesResult&&);
  ~DescriptionAndStylesResult();

  // The parse error, if any. If a parsing error was encountered, no results
  // will be populated.
  std::string error;
  // The parsed descriptions and styles, if parsing was successful.
  std::vector<DescriptionAndStyles> descriptions_and_styles;
};

using DescriptionAndStylesCallback =
    base::OnceCallback<void(DescriptionAndStylesResult)>;

// Parses `str`, which is the suggestion string passed from the extension that
// potentially contains XML markup (e.g., the string may be
// "visit <url>https://example.com</url>"). This parses the string in an
// isolated process and asynchronously returns the parse result via `callback`.
void ParseDescriptionAndStyles(std::string_view str,
                               DescriptionAndStylesCallback callback);
// Same as above, but takes in multiple string inputs.
void ParseDescriptionsAndStyles(const std::vector<std::string_view>& strs,
                                DescriptionAndStylesCallback callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_SUGGESTION_PARSER_H_

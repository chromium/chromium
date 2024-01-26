// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "url/gurl.h"

namespace browser_switcher {

// State of a completed/failed |ParseIeemXml()| call.
class ParsedXml {
 public:
  ParsedXml();
  ParsedXml(ParsedXml&&);
  ParsedXml(RawRuleSet&& rules, std::optional<std::string>&& error);
  ParsedXml(std::vector<std::string>&& sitelist,
            std::vector<std::string>&& greylist,
            std::optional<std::string>&& error);
  ~ParsedXml();

  ParsedXml(const ParsedXml&) = delete;
  ParsedXml& operator=(const ParsedXml&) = delete;

  ParsedXml& operator=(ParsedXml&&) = default;

  RawRuleSet rules;
  std::optional<std::string> error;
};

// Parses the XML contained in |xml|, and calls |callback| with the parsed XML
// result.
void ParseIeemXml(const std::string& xml,
                  ParsingMode parsing_mode,
                  base::OnceCallback<void(ParsedXml)>);

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_

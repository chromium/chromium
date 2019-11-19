// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace browser_switcher {

// State of a completed/failed |ParseIeemXml()| call.
class ParsedXml {
 public:
  ParsedXml();
  ParsedXml(ParsedXml&&);
  ParsedXml(std::vector<std::string>&& rules,
            base::Optional<std::string>&& error);
  ~ParsedXml();

  std::vector<std::string> rules;
  base::Optional<std::string> error;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParsedXml);
};

// Parses the XML contained in |xml|, and calls |callback| with the parsed XML
// result.
void ParseIeemXml(const std::string& xml, base::OnceCallback<void(ParsedXml)>);

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_IEEM_SITELIST_PARSER_H_

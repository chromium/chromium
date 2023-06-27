// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace profile_management {

// Utility class that retrieves attributes from a SAML response found in the
// body of a web page.
class SAMLResponseParser {
 public:
  // Keys for standard attributes that are added to the map by default.
  static constexpr char kDestinationUrl[] = "destination_url";

  using ResponseParserCallback =
      base::OnceCallback<void(const base::flat_map<std::string, std::string>&)>;
  // Decode SAML response from web request `body` and retrieve `attributes` from
  // the response. Invoke `callback` with a map the attributes values.
  SAMLResponseParser(std::vector<std::string>&& attributes,
                     const std::string& body,
                     ResponseParserCallback callback);
  SAMLResponseParser(const SAMLResponseParser&) = delete;
  SAMLResponseParser& operator=(const SAMLResponseParser&) = delete;
  ~SAMLResponseParser();

 private:
  void GetSamlResponse(data_decoder::DataDecoder::ValueOrError value_or_error);

  void GetAttributesFromSAMLResponse(
      data_decoder::DataDecoder::ValueOrError value_or_error);

  std::vector<std::string> attributes_;
  ResponseParserCallback callback_;
  base::WeakPtrFactory<SAMLResponseParser> weak_ptr_factory_{this};
};

}  // namespace profile_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_

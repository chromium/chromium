// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace profile_management {

// Utility class that retrieves attributes from a SAML response found in the
// body of a web page.
class SAMLResponseParser {
 public:
  // Decode SAML response from web request `body` and retrieve `attributes` from
  // the response. Invoke `callback` with a map the attributes values.
  SAMLResponseParser(
      std::vector<std::string>&& attributes,
      const mojo::DataPipeConsumerHandle& body,
      base::OnceCallback<void(base::flat_map<std::string, std::string>)>
          callback);
  SAMLResponseParser(const SAMLResponseParser&) = delete;
  SAMLResponseParser& operator=(const SAMLResponseParser&) = delete;
  ~SAMLResponseParser();

 private:
  void OnBodyReady(MojoResult result);
  void GetSamlResponse(data_decoder::DataDecoder::ValueOrError value_or_error);

  void GetAttributesFromSAMLResponse(
      data_decoder::DataDecoder::ValueOrError value_or_error);

  std::vector<std::string> attributes_;
  const mojo::DataPipeConsumerHandle& body_;
  mojo::SimpleWatcher body_consumer_watcher_;
  base::OnceCallback<void(base::flat_map<std::string, std::string>)> callback_;
  base::WeakPtrFactory<SAMLResponseParser> weak_ptr_factory_{this};
};

}  // namespace profile_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_SAML_RESPONSE_PARSER_H_

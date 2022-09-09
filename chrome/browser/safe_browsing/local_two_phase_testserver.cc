// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/local_two_phase_testserver.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/python_utils.h"

namespace safe_browsing {

namespace {

// Computes the SHA-1 of input string, and returns it as an ASCII-encoded string
// of hex characters.
std::string SHA1HexEncode(const std::string& in) {
  std::string raw_sha1 = base::SHA1HashString(in);
  return base::ToLowerASCII(base::HexEncode(raw_sha1.c_str(), raw_sha1.size()));
}

const char kStartHeader[] = "x-goog-resumable";

std::unique_ptr<net::test_server::HttpResponse> HandleTwoPhaseRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  GURL url = request.GetURL();
  if (request.method == net::test_server::METHOD_POST) {
    const auto start_header = request.headers.find(kStartHeader);
    if (start_header == request.headers.end()) {
      response->set_code(net::HTTP_BAD_REQUEST);
      LOG(WARNING) << "Missing header: " << kStartHeader;
      return response;
    }
    if (start_header->second != "start") {
      response->set_code(net::HTTP_BAD_REQUEST);
      LOG(WARNING) << "Invalid " << kStartHeader
                   << " value: " << start_header->second;
      return response;
    }

    std::string medadata_hash = SHA1HexEncode(request.content);

    // Default response code.
    int p1code = 201;

    std::string p2close = "";
    std::string p2code = "200";

    for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
      // Hang up without sending data, in the case of "p1close".
      if (it.GetKey() == "p1close")
        return std::make_unique<net::test_server::RawHttpResponse>("", "");

      if (it.GetKey() == "p1code") {
        CHECK(base::StringToInt(it.GetValue(), &p1code));
      }
      if (it.GetKey() == "p2close")
        p2close = "1";
      if (it.GetKey() == "p2code")
        p2code = std::string(it.GetValue());
    }

    std::string put_url = base::StringPrintf(
        "%sput?/%s,%s,%s,%s", request.base_url.spec().c_str(),
        url.path().c_str(), medadata_hash.c_str(), p2close.c_str(),
        p2code.c_str());
    response->set_code(static_cast<net::HttpStatusCode>(p1code));
    response->AddCustomHeader("Location", put_url);
    return response;
  }
  if (request.method == net::test_server::METHOD_PUT) {
    if (url.path_piece() != "/put") {
      response->set_code(net::HTTP_BAD_REQUEST);
      LOG(WARNING) << "Invalid path on 2nd phase: " << url.path();
      return response;
    }

    std::vector<std::string> args =
        base::SplitString(url.query_piece().substr(1), ",",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (args.size() != 4) {
      response->set_code(net::HTTP_BAD_REQUEST);
      LOG(WARNING) << "Invalid query string 2nd phase: " << url.query();
      return response;
    }

    std::string initial_path = args[0];
    std::string metadata_hash = args[1];
    std::string p2close = args[2];
    int p2code = 200;
    CHECK(base::StringToInt(args[3], &p2code));

    // Hang up without sending data, in the case of "p2close".
    if (!p2close.empty())
      return std::make_unique<net::test_server::RawHttpResponse>("", "");

    response->set_code(static_cast<net::HttpStatusCode>(p2code));
    response->set_content(base::StringPrintf(
        "%s\n%s\n%s\n", initial_path.c_str(), metadata_hash.c_str(),
        SHA1HexEncode(request.content).c_str()));
    return response;
  }
  response->set_code(net::HTTP_BAD_REQUEST);
  LOG(WARNING) << "Unexpected method: " << request.method_string;
  return response;
}

}  // namespace

LocalTwoPhaseTestServer::LocalTwoPhaseTestServer()
    : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTP) {
  embedded_test_server_.RegisterRequestHandler(
      base::BindRepeating(&HandleTwoPhaseRequest));
}

LocalTwoPhaseTestServer::~LocalTwoPhaseTestServer() {}

}  // namespace safe_browsing

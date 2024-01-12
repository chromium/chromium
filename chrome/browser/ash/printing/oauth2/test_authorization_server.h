// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_TEST_AUTHORIZATION_SERVER_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_TEST_AUTHORIZATION_SERVER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

// Helper function that parses parameters formatted as URL query. Returns false
// <=> the parsing failed (at least one of the keys is empty or repeated). The
// results are saved to `results` that is cleared at the beginning.
bool ParseURLParameters(const std::string& params_str,
                        base::flat_map<std::string, std::string>& results);

// Represents results returned by callback void(StatusCode, const string&).
struct CallbackResult {
  StatusCode status = StatusCode::kUnexpectedError;
  std::string data;
};

// Helper function that returns callback saving its parameters to a given
// structure.
StatusCallback BindResult(CallbackResult& target);

// Builds metadata returned in the response for Metadata request.
base::Value::Dict BuildMetadata(const std::string& authorization_server_uri,
                                const std::string& authorization_uri,
                                const std::string& token_uri,
                                const std::string& registration_uri = "",
                                const std::string& revocation_uri = "");

// Simulates Authorization Server. It contains base::test::TaskEnvironment that
// can be accessed via TaskEnvironment() method.
class FakeAuthorizationServer {
 public:
  FakeAuthorizationServer();
  ~FakeAuthorizationServer();

  base::test::TaskEnvironment& TaskEnvironment() { return task_environment_; }

  // Returns the URLLoaderFactory that must be used to send HTTP requests and
  // receive responses.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return fake_server_.GetSafeWeakWrapper();
  }

  // Processes all pending tasks and get the next request from the fake server.
  // Checks if it is a GET `url` request without a payload. The method returns
  // an empty string <=> there is no errors. Otherwise it returns an error
  // message.
  std::string ReceiveGET(const std::string& url);

  // Processes all pending tasks and get the next request from the fake server.
  // Checks if it is a POST `url` request with a JSON payload. The payload is
  // parsed and saved to `out_params` that is always overwritten. The method
  // returns an empty string <=> there is no errors. Otherwise it returns
  // an error message.
  std::string ReceivePOSTWithJSON(const std::string& url,
                                  base::Value::Dict& out_params);

  // Processes all pending tasks and get the next request from the fake server.
  // Checks if it is a POST `url` request with a payload containing URL-encoded
  // parameters. The payload is parsed and saved to `out_params` that is always
  // overwritten. The method returns an empty string <=> there is no errors.
  // Otherwise it returns an error message.
  std::string ReceivePOSTWithURLParams(
      const std::string& url,
      base::flat_map<std::string, std::string>& out_params);

  // Sends a response to the current pending request with the JSON content given
  // in `params` and processes all pending tasks. The call to this method must
  // be preceded by a previous call to one of Receive* methods.
  void ResponseWithJSON(net::HttpStatusCode status,
                        const base::Value::Dict& params);

 private:
  // Processes all pending tasks and get the next request from the fake server.
  // Returns an error message if there is no requests or the request doesn't
  // match `method`, `url` and `content_type. The payload is saved to `content`.
  // The method returns an empty string <=> there is no errors.
  std::string GetNextRequest(const std::string& method,
                             const std::string& url,
                             const std::string& content_type,
                             std::string& content);

  // Payloads of the incoming requests are stored here (FIFO).
  base::queue<std::string> upload_data_;

  // The pending request that is currently being processed.
  raw_ptr<network::TestURLLoaderFactory::PendingRequest, DanglingUntriaged>
      current_request_ = nullptr;

  network::TestURLLoaderFactory fake_server_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_TEST_AUTHORIZATION_SERVER_H_

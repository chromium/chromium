// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_HTTP_EXCHANGE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_HTTP_EXCHANGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace net {
struct PartialNetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

enum class ContentFormat {
  // Means there is no payloads.
  kEmpty = 0,
  // The payload contains a single JSON object.
  kJson,
  // The payload contains parameters saved in the x-www-form-urlencoded
  // format (see Appendix B in rfc6749).
  kXWwwFormUrlencoded
};

// This class is responsible for creating and sending a HTTP request and
// process a corresponding HTTP response. The following conditions must be met:
// * the payload of HTTP request is one of the types defined in ContentFormat,
// * the payload of HTTP response contains single JSON object.
//
// Usage:
// 1. Create an instance of HttpExchange.
// 2. Specify the content of the request by calling AddParam*(...) methods.
// 3. Call Exchange(...) to initiate the HTTP exchange.
// 4. When the callback given to Exchange(...) returns with `status` parameter
//    equals StatusCode:kOK, the JSON content of the response's payload can be
//    examined by calling methods Param*(...).
// 5. The method GetErrorMessage() returns an error message describing the last
//    reported error.
// 6. The method Clear() reset the HttpExchange object to the initial state, so
//    the whole cycle can be repeated.
class HttpExchange {
 public:
  // This type of callback is returned to the caller when the HTTP response is
  // returned and parsed or when an error occurs.
  using OnExchangeCompletedCallback =
      base::OnceCallback<void(StatusCode status)>;

  // Constructor.
  explicit HttpExchange(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Copying and moving is not allowed.
  HttpExchange(const HttpExchange&) = delete;
  HttpExchange& operator=(const HttpExchange&) = delete;

  // Destructor.
  ~HttpExchange();

  // Deletes internal SimpleURLLoader, all parsed or specified content and
  // error messages. After calling this method the object is in the same
  // initial state as after construction.
  void Clear();

  // Define the request's parameter of type string.
  void AddParamString(const std::string& name, const std::string& value);
  // Define the request's parameter of type array of strings. Works only
  // for requests with JSON content.
  void AddParamArrayString(const std::string& name,
                           const std::vector<std::string>& value);

  // Builds and sends HTTP request and returns. The result of the request is
  // signaled by calling the provided `callback`. One of the following statuses
  // is returned:
  //  * kUnexpectedError
  //  * kConnectionError
  //  * kServerError (HTTP status == 500)
  //  * kServerTemporarilyUnavailable (HTTP status == 503)
  //  * kInvalidResponse
  //  * kAccessDenied (HTTP status == `error_http_status` and
  //    error != "invalid_grant")
  //  * kInvalidAccessToken (HTTP status == `error_http_status` and
  //    error == "invalid_grant")
  //  * kOK (HTTP status == `success_http_status`)
  void Exchange(
      const std::string& http_method,
      const GURL& url,
      ContentFormat request_format,
      int success_http_status,
      int error_http_status,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation,
      OnExchangeCompletedCallback callback);

  // This method is called by an internal instance of SimpleURLLoader when
  // the response is received or an error occurred. This method should not be
  // called directly.
  void OnURLLoaderCompleted(int success_http_status,
                            int error_http_status,
                            OnExchangeCompletedCallback callback,
                            std::unique_ptr<std::string> response_body);

  // Returns the status code from the HTTP response or 0 when the status code
  // cannot be obtained.
  int GetHttpStatus() const;

  // Checks for the array field `name` containing at least one element.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field is not an array
  // * the array in the field does not contain `value` (must be string).
  bool ParamArrayStringContains(const std::string& name,
                                bool required,
                                const std::string& value);
  // Checks for the array field `name` containing strings.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field is not an array
  // * the array in the field is different than `value` (must be an array of
  //   strings).
  bool ParamArrayStringEquals(const std::string& name,
                              bool required,
                              const std::vector<std::string>& value);
  // Checks for the string field `name`.
  // If the field exists and is a string its value is stored in `value`.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field contains an empty string and `required` == true
  // * the field is not a string.
  bool ParamStringGet(const std::string& name,
                      bool required,
                      std::string* value);
  // Checks for the string field `name` and compares it with `value`.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field is not a string
  // * the field contains different string than `value`.
  bool ParamStringEquals(const std::string& name,
                         bool required,
                         const std::string& value);
  // Checks for the string field `name` containing URL of type "https://".
  // If the field exists and has correct type its value is stored in `value`.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field is not a string
  // * the field contains invalid URL or URL with the schema other than https.
  bool ParamURLGet(const std::string& name, bool required, GURL* value);
  // Checks for the string field `name` and compares it with `value`.
  // Sets the error message and returns false when one of the following occurs:
  // * the field is missing and `required` == true
  // * the field is not a string
  // * the field contains URL different than `value`.
  bool ParamURLEquals(const std::string& name,
                      bool required,
                      const GURL& value);

  // Returns the message for the last error.
  const std::string& GetErrorMessage() const;

 private:
  // Returns the pointer to the field `name` in the content or nullptr if there
  // is no fields with this name. Sets also the error message when the field is
  // missing and `required` equals true.
  base::Value* FindNode(const std::string& name, bool required);

  // State that is set during construction.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // While a fetch is in progress.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Stores parameters for a request or parameters parsed from a response.
  base::Value::Dict content_;

  // Error message.
  std::string error_msg_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_HTTP_EXCHANGE_H_

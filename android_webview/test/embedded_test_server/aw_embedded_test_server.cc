// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "android_webview/test/embedded_test_server/aw_net_jni_headers/AwEmbeddedTestServerImpl_jni.h"
#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::ParseQuery;
using net::test_server::RequestQuery;

namespace android_webview {
namespace test {

namespace {
// /click-redirect?url=URL&header=HEADER
// Returns a href redirect to URL.
// Responds in the message body with the headers echoed on the current request.
std::unique_ptr<HttpResponse> HandleClickRedirect(const HttpRequest& request) {
  if (!ShouldHandle(request, "/click-redirect"))
    return nullptr;
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->AddCustomHeader("Cache-Control", "no-cache, no-store");

  std::string url;
  if (query.find("url") != query.end()) {
    url = query.at("url").front();
  }

  std::string content;
  if (query.find("header") != query.end()) {
    for (const auto& header : query.at("header")) {
      std::string header_value = "None";
      if (request.headers.find(header) != request.headers.end())
        header_value = request.headers.at(header);
      content += header_value + "\n";
    }
  }

  http_response->set_content(base::StringPrintf(
      "<html><body><div>%s</div>"
      "<a id=\"click\" href=\"%s\">Click to redirect to %s</a></body></html>",
      content.c_str(), url.c_str(), url.c_str()));

  return std::move(http_response);
}

// /echoheader-and-set-data?header=HEADER1&data=DATA
// Responds as json in the message body with the headers echoed on the current
// request and a content that matches DATA.
std::unique_ptr<HttpResponse> HandleEchoHeaderAndSetData(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/echoheader-and-set-data"))
    return nullptr;
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::string header_content;
  if (query.find("header") != query.end()) {
    for (const auto& header : query.at("header")) {
      std::string header_value = "None";
      if (request.headers.find(header) != request.headers.end())
        header_value = request.headers.at(header);
      if (!header_content.empty())
        header_content += ",";
      header_content += "\"" + header_value + "\"";
    }
  }

  std::string data_content;
  if (query.find("data") != query.end()) {
    for (const auto& data : query.at("data")) {
      if (!data_content.empty())
        data_content += ", ";
      data_content += "\"" + data + "\"";
    }
  }

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("application/json");
  http_response->AddCustomHeader("Cache-Control", "no-cache, no-store");
  http_response->set_content(
      base::StringPrintf("{"
                         "  \"echo header\": [%s],"
                         "  \"data content\": [%s]"
                         "}",
                         header_content.c_str(), data_content.c_str()));

  return std::move(http_response);
}

// /server-redirect-echoheader?url=URL&header=HEADER
// Returns a server-redirect (301) to URL. Pass the headers echoed on the
// current request into the URL request.
std::unique_ptr<HttpResponse> HandleServerRedirectEchoHeader(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/server-redirect-echoheader"))
    return nullptr;
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::string url;
  if (query.find("url") != query.end()) {
    url = query.at("url").front();
  }

  std::string url_suffix;
  if (query.find("header") != query.end()) {
    for (const auto& header : query.at("header")) {
      std::string header_value = "None";
      if (request.headers.find(header) != request.headers.end())
        header_value = request.headers.at(header);
      url_suffix += "&data=" + header_value;
    }
  }
  url.append(url_suffix);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", url);
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><body>Redirecting to %s</body></html>", url.c_str()));
  return std::move(http_response);
}

// /image-response-if-header-not-exists?resource=RESOURCE&header=HEADER
// Returns the response with the base64 encoded image resource in the request.
std::unique_ptr<HttpResponse> HandleSetImageResponse(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/image-response-if-header-not-exists"))
    return nullptr;
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::string resource;
  if (query.find("resource") != query.end()) {
    resource = query.at("resource").front();
  }

  bool header_exist = false;
  if (query.find("header") != query.end()) {
    for (const auto& header : query.at("header")) {
      if (request.headers.find(header) != request.headers.end()) {
        header_exist = true;
        break;
      }
    }
  }

  std::string decoded_resource;
  if (!base::Base64Decode(resource, &decoded_resource))
    return nullptr;

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("image/png");
  http_response->AddCustomHeader("Cache-Control", "no-store");
  if (!header_exist)
    http_response->set_content(decoded_resource);
  else {
    http_response->set_code(net::HTTP_NOT_FOUND);
    http_response->set_content("Found Extra Header. Validation Failed.");
  }
  return std::move(http_response);
}

// /image-onload-html?imagesrc=URL&header=HEADER
// Returns the response with the base64 encoded image resource in the request.
std::unique_ptr<HttpResponse> HandleImageOnloadHtml(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/image-onload-html"))
    return nullptr;
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::string image_url;
  if (query.find("imagesrc") != query.end()) {
    image_url = query.at("imagesrc").front();
  }

  std::string content;
  if (query.find("header") != query.end()) {
    for (const auto& header : query.at("header")) {
      std::string header_value = "None";
      if (request.headers.find(header) != request.headers.end())
        header_value = request.headers.at(header);
      content += header_value + "\n";
    }
  }

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head><script>function updateTitle() "
      "{ document.title=document.getElementById('img').naturalHeight } "
      "</script></head><body><div>%s</div>"
      "<div onload='updateTitle();'><img id='img' onload='updateTitle();' "
      "src='%s'></div></body></html>",
      content.c_str(), image_url.c_str()));
  return std::move(http_response);
}

}  // namespace

// static
ScopedJavaLocalRef<jlongArray> JNI_AwEmbeddedTestServerImpl_GetHandlers(
    JNIEnv* env) {
  std::vector<int64_t> handlers = {
      reinterpret_cast<int64_t>(&HandleClickRedirect),
      reinterpret_cast<int64_t>(&HandleEchoHeaderAndSetData),
      reinterpret_cast<int64_t>(&HandleServerRedirectEchoHeader),
      reinterpret_cast<int64_t>(&HandleSetImageResponse),
      reinterpret_cast<int64_t>(&HandleImageOnloadHtml)};
  return base::android::ToJavaLongArray(env, handlers);
}

}  // namespace test
}  // namespace android_webview

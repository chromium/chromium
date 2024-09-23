// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string_view>

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/client_hints.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/embedded_test_server/aw_net_jni_headers/AwEmbeddedTestServerImpl_jni.h"

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

  const GURL request_url = request.GetURL();
  const RequestQuery query = ParseQuery(request_url);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->AddCustomHeader("Cache-Control", "no-cache, no-store");

  std::string url;
  auto url_query_iter = query.find("url");
  if (url_query_iter != query.end() && !url_query_iter->second.empty()) {
    url = url_query_iter->second.front();
  }

  std::string content;
  auto header_query_iter = query.find("header");
  if (header_query_iter != query.end() && !header_query_iter->second.empty()) {
    for (const std::string& header : header_query_iter->second) {
      std::string header_value = "None";
      auto header_iter = request.headers.find(header);
      if (header_iter != request.headers.end()) {
        header_value = header_iter->second;
      }
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

  const GURL request_url = request.GetURL();
  const RequestQuery query = ParseQuery(request_url);

  std::string header_content;
  auto header_query_iter = query.find("header");
  if (header_query_iter != query.end() && !header_query_iter->second.empty()) {
    for (const std::string& header : header_query_iter->second) {
      auto header_iter = request.headers.find(header);
      std::string header_value =
          (header_iter != request.headers.end()) ? header_iter->second : "None";
      if (!header_content.empty())
        header_content += ",";
      header_content += "\"" + header_value + "\"";
    }
  }

  std::string data_content;
  auto data_query_iter = query.find("data");
  if (data_query_iter != query.end() && !data_query_iter->second.empty()) {
    for (const std::string& data : data_query_iter->second) {
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

  const GURL request_url = request.GetURL();
  const RequestQuery query = ParseQuery(request_url);

  std::string url;
  auto url_query_iter = query.find("url");
  if (url_query_iter != query.end() && !url_query_iter->second.empty()) {
    url = url_query_iter->second.front();
  }

  std::string url_suffix;
  auto header_query_iter = query.find("header");
  if (header_query_iter != query.end() && !header_query_iter->second.empty()) {
    for (const std::string& header : header_query_iter->second) {
      auto header_iter = request.headers.find(header);
      std::string header_value =
          (header_iter != request.headers.end()) ? header_iter->second : "None";
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

  const GURL request_url = request.GetURL();
  const RequestQuery query = ParseQuery(request_url);

  std::string resource;
  auto resource_query_iter = query.find("resource");
  if (resource_query_iter != query.end() &&
      !resource_query_iter->second.empty()) {
    resource = resource_query_iter->second.front();
  }

  bool header_exist = false;
  auto header_query_iter = query.find("header");
  if (header_query_iter != query.end() && !header_query_iter->second.empty()) {
    for (const std::string& header : header_query_iter->second) {
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
  if (!header_exist) {
    http_response->set_content(decoded_resource);
  } else {
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

  const GURL request_url = request.GetURL();
  const RequestQuery query = ParseQuery(request_url);

  std::string image_url;
  auto imagesrc_query_iter = query.find("imagesrc");
  if (imagesrc_query_iter != query.end() &&
      !imagesrc_query_iter->second.empty()) {
    image_url = imagesrc_query_iter->second.front();
  }

  std::string content;
  auto header_query_iter = query.find("header");
  if (header_query_iter != query.end() && !header_query_iter->second.empty()) {
    for (const std::string& header : header_query_iter->second) {
      std::string header_value = "None";
      auto header_value_iter = request.headers.find(header);
      if (header_value_iter != request.headers.end()) {
        header_value = header_value_iter->second;
      }
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

std::unique_ptr<HttpResponse> HandleClientHintsHeaderResponseHelper(
    const HttpRequest& request,
    std::string_view accept_ch,
    std::string_view critical_ch) {
  // Setup basic response.
  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->AddCustomHeader("Cache-Control", "no-cache, no-store");

  // Add relevant headers.
  if (accept_ch.size()) {
    http_response->AddCustomHeader("Accept-CH", accept_ch);
  }
  if (critical_ch.size()) {
    http_response->AddCustomHeader("Critical-CH", critical_ch);
  }

  // Reflect any client hint headers in the request.
  std::string client_hint_dict;
  for (const auto& client_hint_name : network::GetClientHintToNameMap()) {
    const auto& client_hint_value =
        request.headers.find(client_hint_name.second);
    if (!client_hint_dict.empty()) {
      client_hint_dict += ",";
    }
    if (client_hint_value == request.headers.end()) {
      client_hint_dict +=
          "\"" + client_hint_name.second + "\":\"HEADER_NOT_FOUND\"";
    } else {
      client_hint_dict += "\"" + client_hint_name.second + "\":\"" +
                          client_hint_value->second + "\"";
    }
  }
  http_response->set_content("{" + client_hint_dict + "}");

  return std::move(http_response);
}

// /client-hints-header?accept-ch=ACCEPT-CH
// Returns the response with the requested ACCEPT-CH headers set
// and all client hint request headers reflected into the response.
std::unique_ptr<HttpResponse> HandleClientHintsHeaderResponse(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/client-hints-header")) {
    return nullptr;
  }
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);
  const auto& accept_ch = query.find("accept-ch");
  if (accept_ch == query.end()) {
    return nullptr;
  }
  return HandleClientHintsHeaderResponseHelper(request,
                                               accept_ch->second.front(), "");
}

// /critical-client-hints-header?accept-ch=ACCEPT-CH&critical-ch=CRITICAL-CH
// Returns the response with the requested ACCEPT-CH/CRITICAL-CH headers set
// and all client hint request headers reflected into the response.
std::unique_ptr<HttpResponse> HandleCriticalClientHintsHeaderResponse(
    const HttpRequest& request) {
  if (!ShouldHandle(request, "/critical-client-hints-header")) {
    return nullptr;
  }
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);
  const auto& accept_ch = query.find("accept-ch");
  if (accept_ch == query.end()) {
    return nullptr;
  }
  const auto& critical_ch = query.find("critical-ch");
  if (critical_ch == query.end()) {
    return nullptr;
  }
  return HandleClientHintsHeaderResponseHelper(
      request, accept_ch->second.front(), critical_ch->second.front());
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
      reinterpret_cast<int64_t>(&HandleImageOnloadHtml),
      reinterpret_cast<int64_t>(&HandleClientHintsHeaderResponse),
      reinterpret_cast<int64_t>(&HandleCriticalClientHintsHeaderResponse)};
  return base::android::ToJavaLongArray(env, handlers);
}

}  // namespace test
}  // namespace android_webview

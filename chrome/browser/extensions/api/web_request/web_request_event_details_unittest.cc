// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace keys = extension_web_request_api_constants;
using ExtraInfoSpec = extension_web_request_api_helpers::ExtraInfoSpec;

TEST(WebRequestEventDetailsTest, SetResponseHeaders) {
  const int kFilter =
      extension_web_request_api_helpers::ExtraInfoSpec::RESPONSE_HEADERS;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "X-Chrome-ID-Consistency-Response: Value2\r\n"
      "\r\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers_string));

  {
    // Non-Gaia URL.
    WebRequestInfoInitParams params;
    params.url = GURL("http://www.example.com");
    WebRequestInfo request_info(std::move(params));
    WebRequestEventDetails details(request_info, kFilter);
    details.SetResponseHeaders(request_info, headers.get());
    base::Value::Dict dict =
        details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    const base::Value::List* filtered_headers =
        dict.FindList("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(2u, filtered_headers->size());
    const base::Value::Dict& first_header =
        CHECK_DEREF(filtered_headers)[0].GetDict();
    const base::Value::Dict& second_header =
        CHECK_DEREF(filtered_headers)[1].GetDict();
    EXPECT_EQ("Key1", CHECK_DEREF(first_header.FindString("name")));
    EXPECT_EQ("Value1", CHECK_DEREF(first_header.FindString("value")));
    EXPECT_EQ("X-Chrome-ID-Consistency-Response",
              CHECK_DEREF(second_header.FindString("name")));
    EXPECT_EQ("Value2", CHECK_DEREF(second_header.FindString("value")));
  }

  {
    // Gaia URL.
    WebRequestInfoInitParams params;
    params.url = GaiaUrls::GetInstance()->gaia_url();
    WebRequestInfo gaia_request_info(std::move(params));
    WebRequestEventDetails gaia_details(gaia_request_info, kFilter);
    gaia_details.SetResponseHeaders(gaia_request_info, headers.get());
    base::Value::Dict dict =
        gaia_details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value::List* filtered_headers = dict.FindList("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(1u, filtered_headers->size());
    const base::Value::Dict& header =
        CHECK_DEREF(filtered_headers)[0].GetDict();
    EXPECT_EQ("Key1", CHECK_DEREF(header.FindString("name")));
    EXPECT_EQ("Value1", CHECK_DEREF(header.FindString("value")));
  }
}

std::unique_ptr<WebRequestInfo> CreateFakeRequestInfoWithSSL(
    net::CertStatus status) {
  WebRequestInfoInitParams params;
  params.url = GURL("https://secure.example.com");

  auto request_info = std::make_unique<WebRequestInfo>(std::move(params));

  scoped_refptr<net::X509Certificate> certificate =
      net::CertBuilder::CreateSimpleChain(1)[0]->GetX509CertificateChain();

  net::SSLInfo ssl_info;
  ssl_info.cert = certificate;
  ssl_info.cert_status = status;
  request_info->AddSslInfo(std::make_optional(std::move(ssl_info)));

  return request_info;
}

// Tests that if no SSLInfo is provided (e.g. HTTP request)
// then WebRequestEventDetails will produce SecurityInfo object
// with state insecure and other fields absent according to the explainer
// https://github.com/explainers-by-googlers/security-info-web-request
TEST(WebRequestEventDetailsTest, SetSecurityInfo_Insecure) {
  // No SSLInfo provided .
  WebRequestInfoInitParams params;
  params.url = GURL("http://insecure.example.com");
  WebRequestInfo request_info(std::move(params));

  const int kFilter = ExtraInfoSpec::SECURITY_INFO;
  WebRequestEventDetails details(request_info, kFilter);
  details.SetSecurityInfo(request_info);

  // Check that filter will not remove the necessary keys.
  base::Value::Dict dict =
      details.GetFilteredDict(kFilter, nullptr, std::string(), false);

  const base::Value::Dict* security_info =
      dict.FindDict(keys::kSecurityInfoKey);
  ASSERT_TRUE(security_info);
  EXPECT_EQ("insecure", *security_info->FindString(keys::kStateKey));
  EXPECT_FALSE(security_info->FindList(keys::kCertificatesKey));
}

// Tests that when SSLInfo is provided with cert status error
// then WebRequestEventDetails will produce SecurityInfo object
// with state broken and other fields filled according to the explainer
// https://github.com/explainers-by-googlers/security-info-web-request
TEST(WebRequestEventDetailsTest, SetSecurityInfo_Broken) {
  auto request_info =
      CreateFakeRequestInfoWithSSL(net::CERT_STATUS_DATE_INVALID);

  const int kFilter = ExtraInfoSpec::SECURITY_INFO;
  WebRequestEventDetails details(*request_info, kFilter);
  details.SetSecurityInfo(*request_info);

  // Check that filter will not remove the necessary keys.
  base::Value::Dict dict =
      details.GetFilteredDict(kFilter, nullptr, std::string(), false);

  const base::Value::Dict* security_info =
      dict.FindDict(keys::kSecurityInfoKey);
  ASSERT_TRUE(security_info);
  EXPECT_EQ("broken", *security_info->FindString(keys::kStateKey));
  // Certificate list should still be present even if broken.
  EXPECT_TRUE(security_info->FindList(keys::kCertificatesKey));
}
// Tests that when SSLInfo is provided (e.g. successful https request)
// then WebRequestEventDetails will produce SecurityInfo object
// with state secure and other fields filled according to the explainer
// https://github.com/explainers-by-googlers/security-info-web-request
TEST(WebRequestEventDetailsTest, SetSecurityInfoRawDer_Secure) {
  auto request_info = CreateFakeRequestInfoWithSSL(net::OK);

  const int kFilter =
      ExtraInfoSpec::SECURITY_INFO | ExtraInfoSpec::SECURITY_INFO_RAW_DER;
  WebRequestEventDetails details(*request_info, kFilter);
  details.SetSecurityInfo(*request_info);

  // Check that filter will not remove the necessary keys.
  base::Value::Dict dict =
      details.GetFilteredDict(kFilter, nullptr, std::string(), false);

  const base::Value::Dict* security_info =
      dict.FindDict(keys::kSecurityInfoKey);
  ASSERT_TRUE(security_info);
  EXPECT_EQ("secure", *security_info->FindString(keys::kStateKey));

  const base::Value::List* certificates =
      security_info->FindList(keys::kCertificatesKey);
  ASSERT_TRUE(certificates);
  ASSERT_EQ(1u, certificates->size());

  const base::Value::Dict& leaf_cert = certificates->front().GetDict();

  EXPECT_TRUE(leaf_cert.FindBlob(keys::kRawDerKey));

  const base::Value::Dict* fingerprint =
      leaf_cert.FindDict(keys::kFingerprintKey);

  ASSERT_TRUE(fingerprint);
  EXPECT_TRUE(fingerprint->FindString(keys::kSha256Key));
}

// Test checks that WebRequestEventDetails::GetFilteredDict
// without passing ExtraInfoSpec::SECURITY_INFO as filter
// will erase securityInfo values.
TEST(WebRequestEventDetailsTest, SetSecurityInfo_FilteredOut) {
  auto request_info = CreateFakeRequestInfoWithSSL(net::OK);

  const int kFilter =
      ExtraInfoSpec::SECURITY_INFO | ExtraInfoSpec::SECURITY_INFO_RAW_DER;
  WebRequestEventDetails details(*request_info, kFilter);
  details.SetSecurityInfo(*request_info);

  base::Value::Dict dict = details.GetFilteredDict(
      /*extra_info_spec=*/0, nullptr, std::string(), false);

  const base::Value::Dict* security_info =
      dict.FindDict(keys::kSecurityInfoKey);
  ASSERT_FALSE(security_info);
}

// Test checks that WebRequestEventDetails::GetFilteredDict
// when passing only ExtraInfoSpec::SECURITY_INFO and not
// ExtraInfoSpec::SECURITY_INFO_RAW_DER as filter
// will erase CertificateInfo.rawDER field.
TEST(WebRequestEventDetailsTest, SetSecurityInfoRawDer_FilteredOut) {
  auto request_info = CreateFakeRequestInfoWithSSL(net::OK);

  WebRequestEventDetails details(
      *request_info,
      ExtraInfoSpec::SECURITY_INFO | ExtraInfoSpec::SECURITY_INFO_RAW_DER);
  details.SetSecurityInfo(*request_info);

  base::Value::Dict dict = details.GetFilteredDict(
      ExtraInfoSpec::SECURITY_INFO, nullptr, std::string(), false);

  const base::Value::Dict* security_info =
      dict.FindDict(keys::kSecurityInfoKey);
  ASSERT_TRUE(security_info);
  EXPECT_EQ("secure", *security_info->FindString(keys::kStateKey));

  const base::Value::List* certificates =
      security_info->FindList(keys::kCertificatesKey);
  ASSERT_TRUE(certificates);
  ASSERT_EQ(1u, certificates->size());

  const base::Value::Dict& leaf_cert = certificates->front().GetDict();

  EXPECT_FALSE(leaf_cert.FindBlob(keys::kRawDerKey));

  const base::Value::Dict* fingerprint =
      leaf_cert.FindDict(keys::kFingerprintKey);

  ASSERT_TRUE(fingerprint);
  EXPECT_TRUE(fingerprint->FindString(keys::kSha256Key));
}

}  // namespace extensions

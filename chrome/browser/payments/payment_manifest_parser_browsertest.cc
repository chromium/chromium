// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"
#include "content/public/test/browser_test.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/payments/core/error_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

namespace {

std::string CreatePaymentMethodManifestJson(
    const std::vector<GURL>& web_app_manifest_urls,
    const std::vector<url::Origin>& supported_origins) {
  std::string manifest =
      "{"
      "    \"default_applications\":[";
  for (auto iter = web_app_manifest_urls.begin();
       iter != web_app_manifest_urls.end(); ++iter) {
    manifest += "\"" + iter->spec() + "\"";
    if (iter != web_app_manifest_urls.end() - 1)
      manifest += ",";
  }
  manifest +=
      "    ],"
      "    \"supported_origins\":[";
  for (auto iter = supported_origins.begin(); iter != supported_origins.end();
       ++iter) {
    manifest += "\"" + iter->GetURL().spec() + "\"";
    if (iter != supported_origins.end() - 1)
      manifest += ",";
  }
  manifest += "]}";
  return manifest;
}

}  // namespace

// Test fixture for payment manifest parser.
class PaymentManifestParserTest : public InProcessBrowserTest {
 public:
  PaymentManifestParserTest() : parser_(std::make_unique<ErrorLogger>()) {}

  PaymentManifestParserTest(const PaymentManifestParserTest&) = delete;
  PaymentManifestParserTest& operator=(const PaymentManifestParserTest&) =
      delete;

  ~PaymentManifestParserTest() override = default;

  // Sends the |content| to the utility process to parse as a web app manifest
  // and waits until the utility process responds.
  void ParseWebAppManifest(const std::string& content) {
    base::RunLoop run_loop;
    parser_.ParseWebAppManifest(
        content,
        base::BindOnce(&PaymentManifestParserTest::OnWebAppManifestParsed,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Sends the |content| to the utility process to parse as a payment method
  // manifest and waits until the utility process responds.
  void ParsePaymentMethodManifest(const std::string& content) {
    base::RunLoop run_loop;
    parser_.ParsePaymentMethodManifest(
        GURL("https://alicepay.test/"), content,
        base::BindOnce(
            &PaymentManifestParserTest::OnPaymentMethodManifestParsed,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The parsed web app manifest.
  const std::vector<WebAppManifestSection>& web_app_manifest() const {
    return web_app_manifest_;
  }

  // The parsed web app manifest URLs from the payment method manifest.
  const std::vector<GURL>& web_app_manifest_urls() const {
    return web_app_manifest_urls_;
  }

  // The parsed supported origins from the payment method manifest.
  const std::vector<url::Origin>& supported_origins() const {
    return supported_origins_;
  }

 private:
  // Called after the utility process has parsed the web app manifest.
  void OnWebAppManifestParsed(
      base::OnceClosure resume_test,
      const std::vector<WebAppManifestSection>& web_app_manifest) {
    web_app_manifest_ = std::move(web_app_manifest);
    DCHECK(!resume_test.is_null());
    std::move(resume_test).Run();
  }

  // Called after the utility process has parsed the payment method manifest.
  void OnPaymentMethodManifestParsed(
      base::OnceClosure resume_test,
      const std::vector<GURL>& web_app_manifest_urls,
      const std::vector<url::Origin>& supported_origins) {
    web_app_manifest_urls_ = web_app_manifest_urls;
    supported_origins_ = supported_origins;
    DCHECK(!resume_test.is_null());
    std::move(resume_test).Run();
  }

  PaymentManifestParser parser_;
  std::vector<WebAppManifestSection> web_app_manifest_;
  std::vector<GURL> web_app_manifest_urls_;
  std::vector<url::Origin> supported_origins_;
};

// Handles a a manifest with 100 web app URLs.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, TooManyWebAppUrls) {
  std::vector<GURL> web_app_manifest_urls_in;
  web_app_manifest_urls_in.insert(web_app_manifest_urls_in.begin(),
                                  /*count=*/101,
                                  GURL("https://bobpay.test/manifest.json"));
  std::string json = CreatePaymentMethodManifestJson(
      web_app_manifest_urls_in, std::vector<url::Origin>());
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(web_app_manifest_urls().empty());
}

// Handles a manifest with more than 100 supported origins.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, TooManySupportedOrigins) {
  std::vector<url::Origin> supported_origins_in;
  supported_origins_in.insert(supported_origins_in.begin(), /*count=*/100001,
                              url::Origin::Create(GURL("https://bobpay.test")));
  std::string json = CreatePaymentMethodManifestJson(std::vector<GURL>(),
                                                     supported_origins_in);
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(supported_origins().empty());
}

// Handles a manifest with an insecure supported origin.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, InsecureSupportedOrigin) {
  std::string json = CreatePaymentMethodManifestJson(
      std::vector<GURL>(),
      std::vector<url::Origin>(
          1, url::Origin::Create(GURL("http://bobpay.test"))));
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(supported_origins().empty());
}

// Handles a manifest with an insecure web app manifest URL.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, InsecureWebAppManifestUrl) {
  std::string json = CreatePaymentMethodManifestJson(
      std::vector<GURL>(1, GURL("http://bobpay.test/manifest.json")),
      std::vector<url::Origin>());
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(web_app_manifest_urls().empty());
}

// Handles a manifest with an invalid supported origin.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, InvalidSupportedOrigin) {
  std::string json = CreatePaymentMethodManifestJson(
      std::vector<GURL>(),
      std::vector<url::Origin>(1, url::Origin::Create(GURL())));
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(supported_origins().empty());
}

// Handles a manifest with an invalid web app manifest URL.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, InvalidWebAppManifestUrl) {
  std::string json = CreatePaymentMethodManifestJson(
      std::vector<GURL>(1, GURL()), std::vector<url::Origin>());
  ParsePaymentMethodManifest(json);
  EXPECT_TRUE(web_app_manifest_urls().empty());
}

// Verify that the utility process correctly parses a payment method manifest
// that allows all origins to use the corresponding payment method name.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, AllOriginsSupported) {
  ParsePaymentMethodManifest("{\"supported_origins\": \"*\"}");

  EXPECT_TRUE(web_app_manifest_urls().empty());
  EXPECT_TRUE(supported_origins().empty());
}

// Verify that the utility process correctly parses a payment method manifest
// with default applications and some supported origins.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, UrlsAndOrigins) {
  ParsePaymentMethodManifest(
      "{\"default_applications\": "
      "[\"https://alicepay.test/web-app-manifest.json\"], "
      "\"supported_origins\": [\"https://bobpay.test\"]}");

  EXPECT_EQ(
      std::vector<GURL>(1, GURL("https://alicepay.test/web-app-manifest.json")),
      web_app_manifest_urls());
  EXPECT_EQ(std::vector<url::Origin>(
                1, url::Origin::Create(GURL("https://bobpay.test"))),
            supported_origins());
}

// Verifies a valid web app manifest is parsed correctly.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, WebAppManifest) {
  ParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");

  ASSERT_EQ(1U, web_app_manifest().size());
  EXPECT_EQ("com.bobpay.app", web_app_manifest().front().id);
  EXPECT_EQ(1, web_app_manifest().front().min_version);
  ASSERT_EQ(1U, web_app_manifest().front().fingerprints.size());
  EXPECT_EQ(
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,
                            0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1, 0xB2, 0xB3,
                            0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}),
      web_app_manifest().front().fingerprints.front());
}

// Handles a manifest with too many web app sections.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest,
                       TooManyWebAppManifestSections) {
  std::string manifest = " { \"related_applications\": [";
  std::string web_app_section =
      "{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "}";
  for (int i = 0; i < 100; i++) {
    manifest += web_app_section;
    manifest += ",";
  }
  manifest += web_app_section;  // The 101th (last one) without comma.
  manifest += "]}";
  ParseWebAppManifest(manifest);
  EXPECT_TRUE(web_app_manifest().empty());
}

// Handles a manifest with too many fingerprints.
IN_PROC_BROWSER_TEST_F(PaymentManifestParserTest, TooManyFingerprints) {
  std::string manifest =
      "{   \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [";
  std::string fingerprint =
      "  {   \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "  }";
  for (int i = 0; i < 100; i++) {
    manifest += fingerprint;
    manifest += ",";
  }
  manifest += fingerprint;  // Last one with no comma.
  manifest += "]}]}";

  ParseWebAppManifest(manifest);
  EXPECT_TRUE(web_app_manifest().empty());
}

}  // namespace payments

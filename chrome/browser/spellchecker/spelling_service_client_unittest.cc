// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/browser/spelling_service_client.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct SpellingServiceTestCase {
  const wchar_t* request_text;
  std::string sanitized_request_text;
  SpellingServiceClient::ServiceType request_type;
  net::HttpStatusCode response_status;
  std::string response_data;
  bool success;
  const char* corrected_text;
  std::string language;
};

// A class derived from the SpellingServiceClient class used by the
// SpellingServiceClientTest class. This class sets the URLLoaderFactory so
// tests can control requests and responses.
class TestingSpellingServiceClient : public SpellingServiceClient {
 public:
  TestingSpellingServiceClient()
      : success_(false),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    SetURLLoaderFactoryForTesting(test_shared_loader_factory_);
  }
  ~TestingSpellingServiceClient() {}

  void SetExpectedTextCheckResult(bool success,
                                  const std::string& sanitized_request_text,
                                  const char* text) {
    success_ = success;
    sanitized_request_text_ = sanitized_request_text;
    corrected_text_.assign(base::UTF8ToUTF16(text));
  }

  void VerifyResponse(bool success,
                      const std::u16string& request_text,
                      const std::vector<SpellCheckResult>& results) {
    EXPECT_EQ(success_, success);
    std::u16string text(base::UTF8ToUTF16(sanitized_request_text_));
    for (auto it = results.begin(); it != results.end(); ++it) {
      text.replace(it->location, it->length, it->replacements[0]);
    }
    EXPECT_EQ(corrected_text_, text);
  }

  bool ParseResponseSuccess(const std::string& data) {
    std::vector<SpellCheckResult> results;
    return ParseResponse(data, &results);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  bool success_;
  std::string sanitized_request_text_;
  std::u16string corrected_text_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

// A test class used for testing the SpellingServiceClient class. This class
// implements a callback function used by the SpellingServiceClient class to
// monitor the class calls the callback with expected results.
class SpellingServiceClientTest
    : public testing::TestWithParam<SpellingServiceTestCase> {
 public:
  void OnTextCheckComplete(int tag,
                           bool success,
                           const std::u16string& text,
                           const std::vector<SpellCheckResult>& results) {
    client_.VerifyResponse(success, text, results);
  }

 protected:
  bool GetExpectedCountry(const std::string& language, std::string* country) {
    static const struct {
      const char* language;
      const char* country;
    } kCountries[] = {
        {"af", "ZAF"}, {"en", "USA"},
    };
    for (size_t i = 0; i < std::size(kCountries); ++i) {
      if (!language.compare(kCountries[i].language)) {
        country->assign(kCountries[i].country);
        return true;
      }
    }
    return false;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingSpellingServiceClient client_;
  TestingProfile profile_;
};

}  // namespace

// Verifies that SpellingServiceClient::RequestTextCheck() creates a JSON
// request sent to the Spelling service as we expect. This test also verifies
// that it parses a JSON response from the service and calls the callback
// function. To avoid sending real requests to the service, this test uses a
// subclass of SpellingServiceClient that in turn sets the client's URL loader
// factory to a TestURLLoaderFactory. The client thinks it is issuing real
// network requests, but in fact the responses are entirely under our control
// and no network activity takes place.
// This test also uses a custom callback function that replaces all
// misspelled words with ones suggested by the service so this test can compare
// the corrected text with the expected results. (If there are not any
// misspelled words, |corrected_text| should be equal to |request_text|.)
using Redirects = std::vector<
    std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>;

TEST_P(SpellingServiceClientTest, RequestTextCheck) {
  auto test_case = GetParam();

  PrefService* pref = profile_.GetPrefs();
  pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  pref->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService, true);

  client_.test_url_loader_factory()->ClearResponses();
  net::HttpStatusCode http_status = test_case.response_status;

  auto head = network::mojom::URLResponseHead::New();
  std::string headers(base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
      static_cast<int>(http_status), net::GetHttpReasonPhrase(http_status)));
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "application/json";

  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = test_case.response_data.size();
  GURL expected_request_url = client_.BuildEndpointUrl(test_case.request_type);
  client_.test_url_loader_factory()->AddResponse(
      expected_request_url, std::move(head), test_case.response_data, status,
      Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  net::HttpRequestHeaders intercepted_headers;
  std::string intercepted_body;
  GURL requested_url;
  client_.test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_headers = request.headers;
        intercepted_body = network::GetUploadData(request);
        requested_url = request.url;
      }));
  client_.SetExpectedTextCheckResult(test_case.success,
                                     test_case.sanitized_request_text,
                                     test_case.corrected_text);

  base::Value::List dictionary;
  dictionary.Append(test_case.language);
  pref->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                std::move(dictionary));

  client_.RequestTextCheck(
      &profile_, test_case.request_type,
      base::WideToUTF16(test_case.request_text),
      base::BindOnce(&SpellingServiceClientTest::OnTextCheckComplete,
                     base::Unretained(this), 0));
  task_environment_.RunUntilIdle();

  // Verify that the expected endpoint was hit (REST vs RPC).
  ASSERT_EQ(requested_url.path(), expected_request_url.path());

  // Verify the request content type was JSON. (The Spelling service returns
  // an internal server error when this content type is not JSON.)
  EXPECT_EQ("application/json", intercepted_headers.GetHeader(
                                    net::HttpRequestHeaders::kContentType));

  // Parse the JSON sent to the service, and verify its parameters.
  std::optional<base::Value> value = base::JSONReader::Read(
      intercepted_body, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  const base::Value::Dict& dict = value->GetDict();

  EXPECT_FALSE(dict.FindString("method"));
  EXPECT_FALSE(dict.FindString("apiVersion"));
  const std::string* sanitized_text = dict.FindString("text");
  EXPECT_TRUE(sanitized_text);
  EXPECT_EQ(test_case.sanitized_request_text, *sanitized_text);
  const std::string* language = dict.FindString("language");
  EXPECT_TRUE(language);
  std::string expected_language =
      test_case.language.empty() ? std::string("en") : test_case.language;
  EXPECT_EQ(expected_language, *language);
  std::string expected_country;
  ASSERT_TRUE(GetExpectedCountry(*language, &expected_country));
  const std::string* country = dict.FindString("originCountry");
  EXPECT_TRUE(country);
  EXPECT_EQ(expected_country, *country);
}

INSTANTIATE_TEST_SUITE_P(
    SpellingService,
    SpellingServiceClientTest,
    testing::Values(
        // Test cases for the REST endpoint
        SpellingServiceTestCase{
            L"",
            "",
            SpellingServiceClient::SUGGEST,
            net::HttpStatusCode(500),
            "",
            false,
            "",
            "af",
        },
        SpellingServiceTestCase{
            L"chromebook",
            "chromebook",
            SpellingServiceClient::SUGGEST,
            net::HttpStatusCode(200),
            "{}",
            true,
            "chromebook",
            "af",
        },
        SpellingServiceTestCase{
            L"chrombook",
            "chrombook",
            SpellingServiceClient::SUGGEST,
            net::HttpStatusCode(200),
            "{\n"
            "  \"spellingCheckResponse\": {\n"
            "    \"misspellings\": [{\n"
            "      \"charStart\": 0,\n"
            "      \"charLength\": 9,\n"
            "      \"suggestions\": [{ \"suggestion\": \"chromebook\" }],\n"
            "      \"canAutoCorrect\": false\n"
            "    }]\n"
            "  }\n"
            "}",
            true,
            "chromebook",
            "af",
        },
        SpellingServiceTestCase{
            L"",
            "",
            SpellingServiceClient::SPELLCHECK,
            net::HttpStatusCode(500),
            "",
            false,
            "",
            "en",
        },
        SpellingServiceTestCase{
            L"I have been to USA.",
            "I have been to USA.",
            SpellingServiceClient::SPELLCHECK,
            net::HttpStatusCode(200),
            "{}",
            true,
            "I have been to USA.",
            "en",
        },
        SpellingServiceTestCase{
            L"I have bean to USA.",
            "I have bean to USA.",
            SpellingServiceClient::SPELLCHECK,
            net::HttpStatusCode(200),
            "{\n"
            "  \"spellingCheckResponse\": {\n"
            "    \"misspellings\": [{\n"
            "      \"charStart\": 7,\n"
            "      \"charLength\": 4,\n"
            "      \"suggestions\": [{ \"suggestion\": \"been\" }],\n"
            "      \"canAutoCorrect\": false\n"
            "    }]\n"
            "  }\n"
            "}",
            true,
            "I have been to USA.",
            "en",
        },
        SpellingServiceTestCase{
            L"I\x2019mattheIn'n'Out.",
            "I'mattheIn'n'Out.",
            SpellingServiceClient::SPELLCHECK,
            net::HttpStatusCode(200),
            "{\n"
            "  \"spellingCheckResponse\": {\n"
            "    \"misspellings\": [{\n"
            "      \"charStart\": 0,\n"
            "      \"charLength\": 16,\n"
            "      \"suggestions\":"
            " [{ \"suggestion\": \"I'm at the In'N'Out\" }],\n"
            "      \"canAutoCorrect\": false\n"
            "    }]\n"
            "  }\n"
            "}",
            true,
            "I'm at the In'N'Out.",
            "en",
        }));

// Verify that SpellingServiceClient::IsAvailable() returns true only when it
// can send suggest requests or spellcheck requests.
TEST_F(SpellingServiceClientTest, AvailableServices) {
  const SpellingServiceClient::ServiceType kSuggest =
      SpellingServiceClient::SUGGEST;
  const SpellingServiceClient::ServiceType kSpellcheck =
      SpellingServiceClient::SPELLCHECK;

  // When a user disables spellchecking or prevent using the Spelling service,
  // this function should return false both for suggestions and for spellcheck.
  PrefService* pref = profile_.GetPrefs();
  pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
  pref->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService, false);
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));

  pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  pref->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // For locales supported by the SpellCheck service, this function returns
  // false for suggestions and true for spellcheck. (The comment in
  // SpellingServiceClient::IsAvailable() describes why this function returns
  // false for suggestions.) If there is no language set, then we
  // do not allow any remote.
  pref->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                base::Value::List());

  EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));

  static constexpr const char* kSupported[] = {
      "en-AU", "en-CA", "en-GB", "en-US", "da-DK", "es-ES",
  };
  // If spellcheck is allowed, then suggest is not since spellcheck is a
  // superset of suggest.
  for (size_t i = 0; i < std::size(kSupported); ++i) {
    base::Value::List dictionary;
    dictionary.Append(kSupported[i]);
    pref->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                  std::move(dictionary));

    EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
    EXPECT_TRUE(client_.IsAvailable(&profile_, kSpellcheck));
  }

  // This function returns true for suggestions for all and false for
  // spellcheck for unsupported locales.
  static constexpr const char* kUnsupported[] = {
      "af-ZA", "bg-BG", "ca-ES", "cs-CZ", "de-DE", "el-GR", "et-EE", "fo-FO",
      "fr-FR", "he-IL", "hi-IN", "hr-HR", "hu-HU", "id-ID", "it-IT", "lt-LT",
      "lv-LV", "nb-NO", "nl-NL", "pl-PL", "pt-BR", "pt-PT", "ro-RO", "ru-RU",
      "sk-SK", "sl-SI", "sh",    "sr",    "sv-SE", "tr-TR", "uk-UA", "vi-VN",
  };
  for (size_t i = 0; i < std::size(kUnsupported); ++i) {
    SCOPED_TRACE(std::string("Expected language ") + kUnsupported[i]);
    base::Value::List dictionary;
    dictionary.Append(kUnsupported[i]);
    pref->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                  std::move(dictionary));

    EXPECT_TRUE(client_.IsAvailable(&profile_, kSuggest));
    EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));
  }
}

// Verify that an error in JSON response from spelling service will result in
// ParseResponse returning false.
TEST_F(SpellingServiceClientTest, ResponseErrorTest) {
  EXPECT_TRUE(client_.ParseResponseSuccess("{\"result\": {}}"));
  EXPECT_FALSE(client_.ParseResponseSuccess("{\"error\": {}}"));
}

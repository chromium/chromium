// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kNormalPolicy[] = R"({
  "service_provider": "box",
  "enterprise_id": "1234567890",
  "domain": "example.com",
  "enable": [
    {
      "url_list": ["*"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
  "disable": [
    {
      "url_list": ["no.text.com", "no.text.no.image.com"],
      "mime_types": ["text/plain"],
    },
    {
      "url_list": ["no.image.com", "no.text.no.image.com"],
      "mime_types": ["image/png"],
    },
  ],
})";

constexpr char kNoProviderSettings[] = R"({
  "enterprise_id": "1234567890",
  "domain": "example.com",
  "enable": [
    {
      "url_list": ["*"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
  "disable": [
    {
     "url_list": ["no.text.com", "no.text.no.image.com"],
     "mime_types": ["text/plain"]
    },
    {
      "url_list": ["no.image.com", "no.text.no.image.com"],
      "mime_types": ["image/png"]
    },
  ],
})";

constexpr char kNoEnterpriseIdSettings[] = R"({
  "service_provider": "box",
  "domain": "example.com",
  "enable": [
    {
      "url_list": ["*"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
  "disable": [
    {
     "url_list": ["no.text.com", "no.text.no.image.com"],
     "mime_types": ["text/plain"]
    },
    {
      "url_list": ["no.image.com", "no.text.no.image.com"],
      "mime_types": ["image/png"]
    },
  ],
})";

constexpr char kNoDomainPolicy[] = R"({
  "service_provider": "box",
  "enterprise_id": "1234567890",
  "enable": [
    {
      "url_list": ["*"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
  "disable": [
    {
      "url_list": ["no.text.com", "no.text.no.image.com"],
      "mime_types": ["text/plain"],
    },
    {
      "url_list": ["no.image.com", "no.text.no.image.com"],
      "mime_types": ["image/png"],
    },
  ],
})";

constexpr char kNoEnablePolicy[] = R"({
  "service_provider": "box",
  "enterprise_id": "1234567890",
  "domain": "example.com",
  "disable": [
    {
      "url_list": ["no.text.com", "no.text.no.image.com"],
      "mime_types": ["text/plain"],
    },
    {
      "url_list": ["no.image.com", "no.text.no.image.com"],
      "mime_types": ["image/png"],
    },
  ],
})";

constexpr char kSpecificSitesPolicy[] = R"({
  "service_provider": "box",
  "enterprise_id": "1234567890",
  "domain": "example.com",
  "enable": [
    {
      "url_list": ["site1.com", "site2.com"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
})";

constexpr char kAllSitePolicy[] = R"({
  "service_provider": "box",
  "enterprise_id": "1234567890",
  "domain": "example.com",
  "enable": [
    {
      "url_list": ["*"],
      "mime_types": ["text/plain", "image/png", "application/zip"],
    },
  ],
})";

std::set<std::string>* NormalMimeTypes() {
  static base::NoDestructor<std::set<std::string>> mime_types{
      {"text/plain", "image/png", "application/zip"}};
  return mime_types.get();
}

std::set<std::string>* NoTextMimeTypes() {
  static base::NoDestructor<std::set<std::string>> mime_types{
      {"image/png", "application/zip"}};
  return mime_types.get();
}

std::set<std::string>* NoImageMimeTypes() {
  static base::NoDestructor<std::set<std::string>> mime_types{
      {"text/plain", "application/zip"}};
  return mime_types.get();
}

std::set<std::string>* NoTextNoImageMimeTypes() {
  static base::NoDestructor<std::set<std::string>> mime_types{
      {"application/zip"}};
  return mime_types.get();
}

constexpr char kNornmalURL[] = "https://normal.com";
constexpr char kNoTextURL[] = "https://no.text.com";
constexpr char kNoImageURL[] = "https://no.image.com";
constexpr char kNoTextNoImageURL[] = "https://no.text.no.image.com";
constexpr char kSpecificSite1URL[] = "https://site1.com";
constexpr char kSpecificSite2URL[] = "https://site2.com";

}  // namespace

struct TestParam {
  TestParam(const char* url,
            const char* settings_value,
            std::set<std::string>* mime_types)
      : url(url),
        settings_value(settings_value),
        expected_mime_types(mime_types) {}

  const char* url;
  const char* settings_value;
  std::set<std::string>* expected_mime_types;
};

class FileSystemServiceSettingsTest : public testing::TestWithParam<TestParam> {
 public:
  GURL url() const { return GURL(GetParam().url); }
  const char* settings_value() const { return GetParam().settings_value; }
  std::set<std::string>* expected_mime_types() const {
    return GetParam().expected_mime_types;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(FileSystemServiceSettingsTest, Test) {
  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  FileSystemServiceSettings service_settings(settings.value(),
                                             *GetServiceProviderConfig());

  auto file_system_settings_opt = service_settings.GetSettings(url());
  bool has_expected_mime_types = expected_mime_types() != nullptr;
  ASSERT_EQ(has_expected_mime_types, file_system_settings_opt.has_value())
      << settings_value();
  if (file_system_settings_opt.has_value()) {
    ASSERT_TRUE(GetServiceProviderConfig()->count("box"));
    const ServiceProvider provider = GetServiceProviderConfig()->at("box");

    const auto& file_system_settings = file_system_settings_opt.value();

    ASSERT_EQ(file_system_settings.mime_types, *expected_mime_types());
    ASSERT_EQ(file_system_settings.home, GURL(provider.file_system->home));
    ASSERT_EQ(file_system_settings.authorization_endpoint,
              GURL(provider.file_system->authorization_endpoint));
    ASSERT_EQ(file_system_settings.token_endpoint,
              GURL(provider.file_system->token_endpoint));
    ASSERT_EQ(file_system_settings.enterprise_id, "1234567890");
    ASSERT_EQ(file_system_settings.max_direct_size,
              provider.file_system->max_direct_size);
    ASSERT_EQ(file_system_settings.scopes,
              std::vector<std::string>(provider.file_system->scopes.begin(),
                                       provider.file_system->scopes.end()));

    if (!file_system_settings.email_domain.empty())
      ASSERT_EQ(file_system_settings.email_domain, "example.com");
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FileSystemServiceSettingsTest,
    testing::Values(
        TestParam(kNornmalURL, kNormalPolicy, NormalMimeTypes()),
        TestParam(kNoTextURL, kNormalPolicy, NoTextMimeTypes()),
        TestParam(kNoImageURL, kNormalPolicy, NoImageMimeTypes()),
        TestParam(kNoTextNoImageURL, kNormalPolicy, NoTextNoImageMimeTypes()),

        TestParam(kSpecificSite1URL, kSpecificSitesPolicy, NormalMimeTypes()),
        TestParam(kSpecificSite2URL, kSpecificSitesPolicy, NormalMimeTypes()),
        TestParam(kNornmalURL, kSpecificSitesPolicy, nullptr),

        TestParam(kNornmalURL, kNoProviderSettings, nullptr),
        TestParam(kNornmalURL, kNoEnterpriseIdSettings, nullptr),
        TestParam(kNornmalURL, kNoDomainPolicy, NormalMimeTypes()),
        TestParam(kNornmalURL, kNoEnablePolicy, nullptr),

        TestParam("https://box.com", kAllSitePolicy, nullptr),
        TestParam("https://www.box.com", kAllSitePolicy, nullptr),

        // Having no URL should never return settings since the patterns will
        // never match.
        TestParam("", kNormalPolicy, nullptr),
        TestParam("", kSpecificSitesPolicy, nullptr),
        TestParam("", kNoProviderSettings, nullptr),
        TestParam("", kNoEnterpriseIdSettings, nullptr),
        TestParam("", kNoDomainPolicy, nullptr),
        TestParam("", kNoEnablePolicy, nullptr),
        TestParam("", kAllSitePolicy, nullptr)));

}  // namespace enterprise_connectors

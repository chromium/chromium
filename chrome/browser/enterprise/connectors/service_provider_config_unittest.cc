// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/service_provider_config.h"

#include "base/json/json_reader.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxFileSize = 50 * 1024 * 1024;

std::vector<std::string> MalwareMimeTypes() {
  return {"application/vnd.microsoft.portable-executable",
          "application/vnd.rar", "application/x-msdos-program",
          "application/zip"};
}

std::vector<std::string> DlpMimeTypes() {
  return {
      "application/gzip",
      "application/msword",
      "application/pdf",
      "application/postscript",
      "application/rtf",
      "application/vnd.google-apps.document.internal",
      "application/vnd.google-apps.spreadsheet.internal",
      "application/vnd.ms-cab-compressed",
      "application/vnd.ms-excel",
      "application/vnd.ms-powerpoint",
      "application/vnd.ms-xpsdocument",
      "application/vnd.oasis.opendocument.text",
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-excel.template.macroenabled.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.ms-word.template.macroenabled.12",
      "application/vnd.rar",
      "application/vnd.wordperfect",
      "application/x-7z-compressed",
      "application/x-bzip",
      "application/x-bzip2",
      "application/x-tar",
      "application/zip",
      "text/csv",
      "text/plain"};
}

}  // namespace

TEST(ServiceProviderConfigTest, CurrentConfig) {
  // Since this class should only be initialized with 1 value for now, all
  // that's needed is a single test on that value checking every field.
  ServiceProviderConfig config(kServiceProviderConfig);

  const ServiceProviderConfig::ServiceProvider* service_provider =
      config.GetServiceProvider("google");
  ASSERT_NE(nullptr, service_provider);

  ASSERT_EQ("https://safebrowsing.google.com/safebrowsing/uploads/scan",
            service_provider->analysis_url());
  ASSERT_TRUE(GURL(service_provider->analysis_url()).is_valid());
  ASSERT_EQ("https://chromereporting-pa.googleapis.com/v1/events",
            service_provider->reporting_url());
  ASSERT_TRUE(GURL(service_provider->reporting_url()).is_valid());

  ASSERT_EQ(2u, service_provider->analysis_tags().size());
  ASSERT_EQ(1u, service_provider->analysis_tags().count("dlp"));
  ASSERT_EQ(1u, service_provider->analysis_tags().count("malware"));

  ASSERT_EQ(kMaxFileSize,
            service_provider->analysis_tags().at("dlp").max_file_size());
  ASSERT_EQ(kMaxFileSize,
            service_provider->analysis_tags().at("malware").max_file_size());

  ASSERT_EQ(DlpMimeTypes(),
            service_provider->analysis_tags().at("dlp").mime_types());
  ASSERT_EQ(MalwareMimeTypes(),
            service_provider->analysis_tags().at("malware").mime_types());

  service_provider = config.GetServiceProvider("box");
  ASSERT_NE(nullptr, service_provider);

  ASSERT_EQ("https://box.com", service_provider->fs_home_url());
  ASSERT_EQ("https://account.box.com/api/oauth2/authorize",
            service_provider->fs_authorization_endpoint());
  ASSERT_EQ("https://api.box.com/oauth2/token",
            service_provider->fs_token_endpoint());
  ASSERT_EQ(20u * 1024 * 1024, service_provider->fs_max_direct_size());
  ASSERT_TRUE(service_provider->fs_scopes().empty());
  ASSERT_EQ(1u, service_provider->fs_disable().size());
  ASSERT_EQ("box.com", service_provider->fs_disable()[0]);
}

}  // namespace enterprise_connectors

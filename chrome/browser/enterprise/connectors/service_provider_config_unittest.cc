// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/service_provider_config.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxFileSize = 50 * 1024 * 1024;

std::vector<const char*> SupportedDlpMimeTypes() {
  return {
      "application/gzip",
      "application/msexcel",
      "application/mspowerpoint",
      "application/msword",
      "application/octet-stream",
      "application/pdf",
      "application/postscript",
      "application/rtf",
      "application/vnd.google-apps.document.internal",
      "application/vnd.google-apps.spreadsheet.internal",
      "application/vnd.ms-cab-compressed",
      "application/vnd.ms-excel",
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-excel.template.macroenabled.12",
      "application/vnd.ms-powerpoint",
      "application/vnd.ms-powerpoint.presentation.macroenabled.12",
      "application/vnd.ms-word",
      "application/vnd.ms-word.document.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.ms-word.template.macroenabled.12",
      "application/vnd.ms-xpsdocument",
      "application/vnd.msword",
      "application/vnd.oasis.opendocument.text",
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
      "application/vnd.rar",
      "application/vnd.wordperfect",
      "application/x-7z-compressed",
      "application/x-bzip",
      "application/x-bzip2",
      "application/x-gzip",
      "application/x-rar-compressed",
      "application/x-tar",
      "application/x-zip-compressed",
      "application/zip"};
}

std::vector<base::FilePath::StringType> SupportedDlpFileTypes() {
  return {FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
          FILE_PATH_LITERAL(".bzip"), FILE_PATH_LITERAL(".cab"),
          FILE_PATH_LITERAL(".csv"),  FILE_PATH_LITERAL(".doc"),
          FILE_PATH_LITERAL(".docx"), FILE_PATH_LITERAL(".eps"),
          FILE_PATH_LITERAL(".gz"),   FILE_PATH_LITERAL(".gzip"),
          FILE_PATH_LITERAL(".htm"),  FILE_PATH_LITERAL(".html"),
          FILE_PATH_LITERAL(".odt"),  FILE_PATH_LITERAL(".pdf"),
          FILE_PATH_LITERAL(".ppt"),  FILE_PATH_LITERAL(".pptx"),
          FILE_PATH_LITERAL(".ps"),   FILE_PATH_LITERAL(".rar"),
          FILE_PATH_LITERAL(".rtf"),  FILE_PATH_LITERAL(".tar"),
          FILE_PATH_LITERAL(".txt"),  FILE_PATH_LITERAL(".wpd"),
          FILE_PATH_LITERAL(".xls"),  FILE_PATH_LITERAL(".xlsx"),
          FILE_PATH_LITERAL(".xps"),  FILE_PATH_LITERAL(".zip")};
}

std::vector<base::FilePath::StringType> UnsupportedDlpFileTypes() {
  return {FILE_PATH_LITERAL(".these"), FILE_PATH_LITERAL(".types"),
          FILE_PATH_LITERAL(".are"), FILE_PATH_LITERAL(".not"),
          FILE_PATH_LITERAL(".supported")};
}

std::vector<std::string> UnsupportedDlpMimeTypes() {
  return {"image/png", "video/webm", "audio/wav", "i/made", "this/up", "foo"};
}

base::FilePath FilePath(const base::FilePath::StringType& type) {
  return base::FilePath(FILE_PATH_LITERAL("foo") + type);
}

}  // namespace

TEST(ServiceProviderConfigTest, CurrentConfig) {
  // Since this class should only be initialized with 1 value for now, all
  // that's needed is a single test on that value checking every field.
  const ServiceProviderConfig* config = GetServiceProviderConfig();

  ASSERT_TRUE(config->count("google"));
  ServiceProvider service_provider = config->at("google");

  ASSERT_EQ("https://safebrowsing.google.com/safebrowsing/uploads/scan",
            std::string(service_provider.analysis->url));
  ASSERT_TRUE(GURL(service_provider.analysis->url).is_valid());
  ASSERT_EQ("https://chromereporting-pa.googleapis.com/v1/events",
            std::string(service_provider.reporting->url));
  ASSERT_TRUE(GURL(service_provider.reporting->url).is_valid());

  ASSERT_EQ(2u, service_provider.analysis->supported_tags.size());
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags.at(0).name),
            "malware");
  ASSERT_EQ(service_provider.analysis->supported_tags.at(0).max_file_size,
            kMaxFileSize);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags.at(1).name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags.at(1).max_file_size,
            kMaxFileSize);
  // Only a subset of mime types and extensions are supported by Google DLP, but
  // every type is supported by malware scanning.
  const auto* malware_supported_files =
      service_provider.analysis->supported_tags.at(0).supported_files;
  const auto* dlp_supported_files =
      service_provider.analysis->supported_tags.at(1).supported_files;
  for (const base::FilePath::StringType& type : SupportedDlpFileTypes()) {
    ASSERT_TRUE(dlp_supported_files->FileExtensionSupported(FilePath(type)));
    ASSERT_TRUE(
        malware_supported_files->FileExtensionSupported(FilePath(type)));
  }
  for (const base::FilePath::StringType& type : UnsupportedDlpFileTypes()) {
    ASSERT_FALSE(dlp_supported_files->FileExtensionSupported(FilePath(type)));
    ASSERT_TRUE(
        malware_supported_files->FileExtensionSupported(FilePath(type)));
  }
  for (const std::string& type : SupportedDlpMimeTypes()) {
    ASSERT_TRUE(dlp_supported_files->MimeTypeSupported(type));
    ASSERT_TRUE(malware_supported_files->MimeTypeSupported(type));
  }
  for (const std::string& type : UnsupportedDlpMimeTypes()) {
    ASSERT_FALSE(dlp_supported_files->MimeTypeSupported(type));
    ASSERT_TRUE(malware_supported_files->MimeTypeSupported(type));
  }

  ASSERT_TRUE(config->count("box"));
  service_provider = config->at("box");

  ASSERT_EQ("https://box.com", std::string(service_provider.file_system->home));
  ASSERT_EQ("https://account.box.com/api/oauth2/authorize",
            std::string(service_provider.file_system->authorization_endpoint));
  ASSERT_EQ("https://api.box.com/oauth2/token",
            std::string(service_provider.file_system->token_endpoint));
  ASSERT_EQ(20u * 1024 * 1024, service_provider.file_system->max_direct_size);
  ASSERT_TRUE(service_provider.file_system->scopes.empty());
  ASSERT_EQ(2u, service_provider.file_system->disable.size());
  ASSERT_EQ("box.com",
            std::string(service_provider.file_system->disable.at(0)));
  ASSERT_EQ("boxcloud.com",
            std::string(service_provider.file_system->disable.at(1)));
}

}  // namespace enterprise_connectors

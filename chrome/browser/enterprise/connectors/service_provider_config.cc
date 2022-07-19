// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "base/json/json_reader.h"

#if defined(USE_OFFICIAL_ENTERPRISE_CONNECTORS_API_KEYS)
#include "google_apis/internal/enterprise_connectors_api_keys.h"
#endif

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

#if !defined(CLIENT_ID_CONNECTOR_PARTNER_BOX)
#define CLIENT_ID_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

#if !defined(CLIENT_SECRET_CONNECTOR_PARTNER_BOX)
#define CLIENT_SECRET_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

namespace enterprise_connectors {

namespace {

class AllFilesAllowed : public SupportedFiles {
 public:
  bool MimeTypeSupported(const std::string& mime_type) const override {
    return true;
  }

  bool FileExtensionSupported(const base::FilePath& path) const override {
    return true;
  }
};

constexpr AllFilesAllowed kAllFilesAllowed;

class GoogleDlpSupportedFiles : public SupportedFiles {
 public:
  bool MimeTypeSupported(const std::string& mime_type) const override {
    // All text mime type are considered scannable for Google DLP.
    if (mime_type.size() >= 5 && mime_type.substr(0, 5) == "text/")
      return true;

    // Keep sorted for efficient access.
    static constexpr const std::array<const char*, 38> kSupportedDLPMimeTypes =
        {"application/gzip",
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
         "application/"
         "vnd.openxmlformats-officedocument.wordprocessingml.document",
         "application/"
         "vnd.openxmlformats-officedocument.wordprocessingml.template",
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
    // TODO: Replace this DCHECK with a static assert once std::is_sorted is
    // constexpr in C++20.
    DCHECK(std::is_sorted(kSupportedDLPMimeTypes.begin(),
                          kSupportedDLPMimeTypes.end(),
                          [](const std::string& a, const std::string& b) {
                            return a.compare(b) < 0;
                          }));

    return std::binary_search(kSupportedDLPMimeTypes.begin(),
                              kSupportedDLPMimeTypes.end(), mime_type);
  }

  bool FileExtensionSupported(const base::FilePath& path) const override {
    // Keep sorted for efficient access.
    static constexpr const std::array<const base::FilePath::CharType*, 26>
        kSupportedDLPFileTypes = {
            FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
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
    // TODO: Replace this DCHECK with a static assert once std::is_sorted is
    // constexpr in C++20.
    DCHECK(std::is_sorted(
        kSupportedDLPFileTypes.begin(), kSupportedDLPFileTypes.end(),
        [](const base::FilePath::StringType& a,
           const base::FilePath::StringType& b) { return a.compare(b) < 0; }));

    base::FilePath::StringType extension(path.FinalExtension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   tolower);

    return std::binary_search(kSupportedDLPFileTypes.begin(),
                              kSupportedDLPFileTypes.end(), extension);
  }
};

constexpr GoogleDlpSupportedFiles kGoogleDlpSupportedFiles;

constexpr std::array<SupportedTag, 2> kGoogleDlpSupportedTags = {{
    {
        .name = "malware",
        .display_name = "Threat protection",
        .max_file_size = 52428800,
        .supported_files = &kAllFilesAllowed,
    },
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
        .supported_files = &kGoogleDlpSupportedFiles,
    },
}};

constexpr AnalysisConfig kGoogleAnalysisConfig = {
    .url = "https://safebrowsing.google.com/safebrowsing/uploads/scan",
    .supported_tags = base::span<const SupportedTag>(kGoogleDlpSupportedTags),
};

constexpr std::array<SupportedTag, 1> kLocalTestSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
        .supported_files = &kAllFilesAllowed,
    },
}};

constexpr AnalysisConfig kLocalTestAnalysisConfig = {
    .local_path = "test_path",
    .supported_tags = base::span<const SupportedTag>(kLocalTestSupportedTags),
    .user_specific = true,
};

constexpr ReportingConfig kGoogleReportingConfig = {
    .url = "https://chromereporting-pa.googleapis.com/v1/events",
};

constexpr FileSystemConfig kBoxFileSystemConfig = {
    .home = "https://box.com",
    .authorization_endpoint = "https://account.box.com/api/oauth2/authorize",
    .token_endpoint = "https://api.box.com/oauth2/token",
    .max_direct_size = 20971520,
    .scopes = {},
    .disable = {"box.com", "boxcloud.com"},
    .client_id = CLIENT_ID_CONNECTOR_PARTNER_BOX,
    .client_secret = CLIENT_SECRET_CONNECTOR_PARTNER_BOX,
};

}  // namespace

const ServiceProviderConfig* GetServiceProviderConfig() {
  static constexpr ServiceProviderConfig kServiceProviderConfig =
      base::MakeFixedFlatMap<base::StringPiece, ServiceProvider>({
          {
              "google",
              {
                  .display_name = "Google Cloud",
                  .analysis = &kGoogleAnalysisConfig,
                  .reporting = &kGoogleReportingConfig,
              },
          },
          {
              "box",
              {
                  .display_name = "Box",
                  .file_system = &kBoxFileSystemConfig,
              },
          },
          // TODO(b/226560946): Add the actual local content analysis service
          // providers to this config.
          {
              "local_test",
              {
                  .display_name = "Local Test",
                  .analysis = &kLocalTestAnalysisConfig,
              },
          },
      });
  return &kServiceProviderConfig;
}

}  // namespace enterprise_connectors

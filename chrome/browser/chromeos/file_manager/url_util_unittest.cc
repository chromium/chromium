// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/url_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/chromeos_features.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

// Parse a JSON query string into a base::Value.
base::Value ParseJsonQueryString(const std::string& query) {
  const std::string json = net::UnescapeURLComponent(
      query, net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
                 net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json);
  return value ? std::move(*value) : base::Value();
}

// Pretty print the JSON escaped in the query string.
std::string PrettyPrintEscapedJson(const std::string& query) {
  std::string pretty_json;
  base::JSONWriter::WriteWithOptions(ParseJsonQueryString(query),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &pretty_json);
  return pretty_json;
}

base::Value MakeSimpleDictionary(
    std::vector<std::pair<std::string, std::string>> entries) {
  std::vector<std::pair<std::string, std::unique_ptr<base::Value>>> storage;
  storage.reserve(entries.size());
  for (auto& entry : entries) {
    storage.emplace_back(std::move(entry.first), std::make_unique<base::Value>(
                                                     std::move(entry.second)));
  }
  return base::Value(std::move(storage));
}

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrl) {
  EXPECT_EQ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/main.html",
            GetFileManagerMainPageUrl().spec());
}

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrlWithParams_NoFileTypes) {
  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::UTF8ToUTF16("some title"),
      GURL("filesystem:chrome-extension://abc/Downloads/"),
      GURL("filesystem:chrome-extension://abc/Downloads/foo.txt"), "foo.txt",
      nullptr,  // No file types
      0,        // Hence no file type index.
      FILE_PATH_LITERAL("txt"));
  EXPECT_EQ(extensions::kExtensionScheme, url.scheme());
  EXPECT_EQ("hhaomjibdihmijegdhdafkllkbggdgoj", url.host());
  EXPECT_EQ("/main.html", url.path());
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // With DriveFS, Drive is always allowed where native paths are.
  EXPECT_EQ(MakeSimpleDictionary({
                {"allowedPaths",
                 base::FeatureList::IsEnabled(chromeos::features::kDriveFs)
                     ? "nativeOrDrivePath"
                     : "nativePath"},
                {"currentDirectoryURL",
                 "filesystem:chrome-extension://abc/Downloads/"},
                {"defaultExtension", "txt"},
                {"selectionURL",
                 "filesystem:chrome-extension://abc/Downloads/foo.txt"},
                {"targetName", "foo.txt"},
                {"title", "some title"},
                {"type", "open-file"},
            }),
            ParseJsonQueryString(url.query()));
}

TEST(FileManagerUrlUtilTest,
     GetFileManagerMainPageUrlWithParams_WithFileTypes) {
  // Create a FileTypeInfo which looks like:
  // extensions: [["htm", "html"], ["txt"]]
  // descriptions: ["HTML", "TEXT"]
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.extensions.emplace_back();
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("htm"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_types.extensions.emplace_back();
  file_types.extensions[1].push_back(FILE_PATH_LITERAL("txt"));
  file_types.extension_description_overrides.push_back(
      base::UTF8ToUTF16("HTML"));
  file_types.extension_description_overrides.push_back(
      base::UTF8ToUTF16("TEXT"));
  // "shouldReturnLocalPath" will be false if drive is supported.
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::UTF8ToUTF16("some title"),
      GURL("filesystem:chrome-extension://abc/Downloads/"),
      GURL("filesystem:chrome-extension://abc/Downloads/foo.txt"),
      "foo.txt",
      &file_types,
      1,  // The file type index is 1-based.
      FILE_PATH_LITERAL("txt"));
  EXPECT_EQ(extensions::kExtensionScheme, url.scheme());
  EXPECT_EQ("hhaomjibdihmijegdhdafkllkbggdgoj", url.host());
  EXPECT_EQ("/main.html", url.path());
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // The escaped query is hard to read. Pretty print the escaped JSON.
  EXPECT_EQ(
      "{\n"
      "   \"allowedPaths\": \"anyPath\",\n"
      "   \"currentDirectoryURL\": "
      "\"filesystem:chrome-extension://abc/Downloads/\",\n"
      "   \"defaultExtension\": \"txt\",\n"
      "   \"includeAllFiles\": false,\n"
      "   \"selectionURL\": "
      "\"filesystem:chrome-extension://abc/Downloads/foo.txt\",\n"
      "   \"targetName\": \"foo.txt\",\n"
      "   \"title\": \"some title\",\n"
      "   \"type\": \"open-file\",\n"
      "   \"typeList\": [ {\n"
      "      \"description\": \"HTML\",\n"
      "      \"extensions\": [ \"htm\", \"html\" ],\n"
      "      \"selected\": true\n"
      "   }, {\n"
      "      \"description\": \"TEXT\",\n"
      "      \"extensions\": [ \"txt\" ],\n"
      "      \"selected\": false\n"
      "   } ]\n"
      "}\n",
      PrettyPrintEscapedJson(url.query()));
}

}  // namespace
}  // namespace util
}  // namespace file_manager

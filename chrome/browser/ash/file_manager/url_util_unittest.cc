// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/url_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace file_manager {
namespace util {
namespace {

// Parse a JSON query string into a base::Value.
base::Value ParseJsonQueryString(const std::string& query) {
  const std::string json = base::UnescapeBinaryURLComponent(query);
  std::optional<base::Value> value = base::JSONReader::Read(json);
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

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrl) {
  EXPECT_EQ(url::Origin::Create(GetFileManagerMainPageUrl()).GetURL(),
            file_manager::util::GetFileManagerURL());
  EXPECT_THAT(
      GetFileManagerMainPageUrl().spec(),
      ::testing::StartsWith(file_manager::util::GetFileManagerURL().spec()));
}

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrlWithParams_NoFileTypes) {
  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_OPEN_FILE, u"some title",
      GURL("filesystem:chrome-extension://abc/Downloads/"),
      GURL("filesystem:chrome-extension://abc/Downloads/foo.txt"), "foo.txt",
      nullptr,  // No file types
      0,        // Hence no file type index.
      "",       // search_query
      false,    // show_android_picker_apps
      {}        // volume_filter
  );

  EXPECT_EQ(url::Origin::Create(url).GetURL(),
            file_manager::util::GetFileManagerURL());
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // With DriveFS, Drive is always allowed where native paths are.
  EXPECT_EQ(base::StringPrintf(
                "{\n"
                "   \"allowedPaths\": \"nativePath\",\n"
                "   \"currentDirectoryURL\": "
                "\"filesystem:chrome-extension://abc/Downloads/\",\n"
                "   \"searchQuery\": \"\",\n"
                "   \"selectionURL\": "
                "\"filesystem:chrome-extension://abc/Downloads/foo.txt\",\n"
                "   \"showAndroidPickerApps\": false,\n"
                "   \"title\": \"some title\",\n"
                "   \"type\": \"open-file\"\n"
                "}\n"),
            PrettyPrintEscapedJson(url.query()));
}

TEST(FileManagerUrlUtilTest,
     GetFileManagerMainPageUrlWithParams_WithFileTypes) {
  ui::SelectFileDialog::FileTypeInfo file_types{
      {{FILE_PATH_LITERAL("htm"), FILE_PATH_LITERAL("html")},
       {FILE_PATH_LITERAL("txt")}},
      {u"HTML", u"TEXT"}};
  // "shouldReturnLocalPath" will be false if drive is supported.
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, u"some title",
      GURL("filesystem:chrome-extension://abc/Downloads/"),
      GURL("filesystem:chrome-extension://abc/Downloads/foo.txt"), "foo.txt",
      &file_types,
      1,  // The file type index is 1-based.
      "search query",
      true,  // show_android_picker_apps
      // Add meaningless volume filter names so we can test they are added
      // to the file manager URL launch parameters.
      {"foo", "bar"});

  EXPECT_EQ(file_manager::util::GetFileManagerURL().scheme(), url.scheme());
  // URL path can be / or /main.html depending on which version of the app is
  // launched. For the legacy, we'd expect /main.html, otherwise, just /.
  EXPECT_THAT(url.path(), ::testing::StartsWith("/"));
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // The escaped query is hard to read. Pretty print the escaped JSON.
  EXPECT_EQ(
      "{\n"
      "   \"allowedPaths\": \"anyPath\",\n"
      "   \"currentDirectoryURL\": "
      "\"filesystem:chrome-extension://abc/Downloads/\",\n"
      "   \"includeAllFiles\": false,\n"
      "   \"searchQuery\": \"search query\",\n"
      "   \"selectionURL\": "
      "\"filesystem:chrome-extension://abc/Downloads/foo.txt\",\n"
      "   \"showAndroidPickerApps\": true,\n"
      "   \"targetName\": \"foo.txt\",\n"
      "   \"title\": \"some title\",\n"
      "   \"type\": \"saveas-file\",\n"
      "   \"typeList\": [ {\n"
      "      \"description\": \"HTML\",\n"
      "      \"extensions\": [ \"htm\", \"html\" ],\n"
      "      \"selected\": true\n"
      "   }, {\n"
      "      \"description\": \"TEXT\",\n"
      "      \"extensions\": [ \"txt\" ],\n"
      "      \"selected\": false\n"
      "   } ],\n"
      "   \"volumeFilter\": [ \"foo\", \"bar\" ]\n"
      "}\n",
      PrettyPrintEscapedJson(url.query()));
}

}  // namespace
}  // namespace util
}  // namespace file_manager

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

std::string convertDataElementToString(const network::DataElement& element) {
  if (element.type() == network::mojom::DataElementType::kBytes) {
    return std::string(element.bytes(), element.length());
  }
  if (element.type() == network::mojom::DataElementType::kFile) {
    return std::string(element.path().AsUTF8Unsafe());
  }
  return "";
}

void CheckDataElements(
    const scoped_refptr<network::ResourceRequestBody>& body,
    const std::vector<network::mojom::DataElementType>& expected_element_types,
    const std::vector<std::string>& expected_element_values) {
  EXPECT_NE(nullptr, body->elements());
  const std::vector<network::DataElement>& data_elements = *body->elements();

  EXPECT_EQ(expected_element_types.size(), data_elements.size());
  EXPECT_EQ(expected_element_types.size(), expected_element_values.size());

  for (size_t i = 0; i < expected_element_types.size(); ++i) {
    EXPECT_EQ(expected_element_types[i], data_elements[i].type())
        << "unexpected difference at i = " << i;
    EXPECT_EQ(expected_element_values[i],
              convertDataElementToString(data_elements[i]))
        << "unexpected difference at i = " << i;
  }
}

// Test that multipart/form-data body is empty if inputs are of different sizes.
TEST(WebApkActivityTest, InvalidMultipartBody) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values;
  std::vector<bool> is_value_file_uris;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      webapk::ComputeMultipartBody(names, values, is_value_file_uris, filenames,
                                   types, boundary);
  EXPECT_EQ(nullptr, multipart_body.get());
}

// Test that multipart/form-data body is correctly computed for accepted
// file inputs.
TEST(WebApkActivityTest, ValidMultipartBodyForFile) {
  std::vector<std::string> names = {"share-file\"", "share-file\""};
  std::vector<std::string> values = {"mock-file-path", "mock-shared-text"};
  std::vector<bool> is_value_file_uris = {true, false};

  std::vector<std::string> filenames = {"filename\r\n", "shared.txt"};
  std::vector<std::string> types = {"type", "type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      webapk::ComputeMultipartBody(names, values, is_value_file_uris, filenames,
                                   types, boundary);

  std::vector<network::mojom::DataElementType> expected_types = {
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kFile,
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data; name=\"share-file%22\"; "
      "filename=\"filename%0D%0A\"\r\nContent-Type: type\r\n\r\n",
      "mock-file-path", "\r\n",
      "--boundary\r\nContent-Disposition: form-data; name=\"share-file%22\"; "
      "filename=\"shared.txt\"\r\nContent-Type: "
      "type\r\n\r\nmock-shared-text\r\n",
      "--boundary--\r\n"};

  CheckDataElements(multipart_body, expected_types, expected);
}

// Test that multipart/form-data body is correctly computed for non-file inputs.
TEST(WebApkActivityTest, ValidMultipartBodyForText) {
  std::vector<std::string> names = {"name\""};
  std::vector<std::string> values = {"value"};
  std::vector<bool> is_value_file_uris = {false};
  std::vector<std::string> filenames = {""};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      webapk::ComputeMultipartBody(names, values, is_value_file_uris, filenames,
                                   types, boundary);

  std::vector<network::mojom::DataElementType> expected_types = {
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name%22\"\r\nContent-Type: type\r\n\r\nvalue\r\n",
      "--boundary--\r\n"};

  CheckDataElements(multipart_body, expected_types, expected);
}

// Test that multipart/form-data body is correctly computed for a mixture
// of file and non-file inputs.
TEST(WebApkActivityTest, ValidMultipartBodyForTextAndFile) {
  std::vector<std::string> names = {"name1\"", "name2", "name3",
                                    "name4",   "name5", "name6"};
  std::vector<std::string> values = {"value1", "file_uri2", "file_uri3",
                                     "value4", "file_uri5", "value6"};
  std::vector<bool> is_value_file_uris = {false, true, true,
                                          false, true, false};
  std::vector<std::string> filenames = {"", "filename2\r\n", "filename3", "",
                                        "", "shared.txt"};
  std::vector<std::string> types = {"type1", "type2", "type3",
                                    "type4", "type5", "type6"};
  std::string boundary = "boundary";

  scoped_refptr<network::ResourceRequestBody> body =
      webapk::ComputeMultipartBody(names, values, is_value_file_uris, filenames,
                                   types, boundary);

  std::vector<network::mojom::DataElementType> expected_types = {
      // item 1
      network::mojom::DataElementType::kBytes,
      // item 2
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kFile,
      network::mojom::DataElementType::kBytes,
      // item 3
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kFile,
      network::mojom::DataElementType::kBytes,
      // item 4
      network::mojom::DataElementType::kBytes,
      // item 5
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kFile,
      network::mojom::DataElementType::kBytes,
      // item 6
      network::mojom::DataElementType::kBytes,
      // ending
      network::mojom::DataElementType::kBytes};
  std::vector<std::string> expected = {
      // item 1
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name1%22\"\r\nContent-Type: type1\r\n\r\nvalue1\r\n",
      // item 2
      "--boundary\r\nContent-Disposition: form-data; name=\"name2\"; "
      "filename=\"filename2%0D%0A\"\r\nContent-Type: type2\r\n\r\n",
      "file_uri2", "\r\n",
      // item 3
      "--boundary\r\nContent-Disposition: form-data; name=\"name3\"; "
      "filename=\"filename3\"\r\nContent-Type: type3\r\n\r\n",
      "file_uri3", "\r\n",
      // item 4
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name4\"\r\nContent-Type: type4\r\n\r\nvalue4\r\n",
      // item 5
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name5\"\r\nContent-Type: type5\r\n\r\n",
      "file_uri5", "\r\n",
      // item 6
      "--boundary\r\nContent-Disposition: form-data; name=\"name6\"; "
      "filename=\"shared.txt\"\r\nContent-Type: type6\r\n\r\nvalue6\r\n",
      "--boundary--\r\n"};
  CheckDataElements(body, expected_types, expected);
}

// Test that multipart/form-data body is properly percent-escaped.
TEST(WebApkActivityTest, MultipartBodyWithPercentEncoding) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values = {"value"};
  std::vector<bool> is_value_file_uris = {false};
  std::vector<std::string> filenames = {"filename"};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> body =
      webapk::ComputeMultipartBody(names, values, is_value_file_uris, filenames,
                                   types, boundary);
  EXPECT_NE(nullptr, body->elements());

  std::vector<network::mojom::DataElementType> expected_types = {
      network::mojom::DataElementType::kBytes,
      network::mojom::DataElementType::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"name\"; filename=\"filename\"\r\nContent-Type: type"
      "\r\n\r\nvalue\r\n",
      "--boundary--\r\n"};

  CheckDataElements(body, expected_types, expected);
}

// Test that application/x-www-form-urlencoded body is empty if inputs are of
// different sizes.
TEST(WebApkActivityTest, InvalidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1"};
  std::string application_body = webapk::ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("", application_body);
}

// Test that application/x-www-form-urlencoded body is correctly computed for
// accepted inputs.
TEST(WebApkActivityTest, ValidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1", "value2"};
  std::string application_body = webapk::ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("name1=value1&name2=value2", application_body);
}

// Test that PercentEscapeString correctly escapes quotes to %22.
TEST(WebApkActivityTest, NeedsPercentEscapeQuote) {
  EXPECT_EQ("hello%22", webapk::PercentEscapeString("hello\""));
}

// Test that PercentEscapeString correctly escapes newline to %0A.
TEST(WebApkActivityTest, NeedsPercentEscape0A) {
  EXPECT_EQ("%0A", webapk::PercentEscapeString("\n"));
}

// Test that PercentEscapeString correctly escapes \r to %0D.
TEST(WebApkActivityTest, NeedsPercentEscape0D) {
  EXPECT_EQ("%0D", webapk::PercentEscapeString("\r"));
}

// Test that Percent Escape is not performed on strings that don't need to be
// escaped.
TEST(WebApkActivityTest, NoPercentEscape) {
  EXPECT_EQ("helloworld", webapk::PercentEscapeString("helloworld"));
}

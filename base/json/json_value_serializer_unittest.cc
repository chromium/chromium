// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Some proper JSON to test with:
const char kProperJSON[] =
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n";

// Some proper JSON with trailing commas:
const char kProperJSONWithCommas[] =
    "{\n"
    "\t\"some_int\": 42,\n"
    "\t\"some_String\": \"1337\",\n"
    "\t\"the_list\": [\"val1\", \"val2\", ],\n"
    "\t\"compound\": { \"a\": 1, \"b\": 2, },\n"
    "}\n";

// kProperJSON with a few misc characters at the begin and end.
const char kProperJSONPadded[] =
    ")]}'\n"
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n"
    "?!ab\n";

const char kWinLineEnds[] = "\r\n";
const char kLinuxLineEnds[] = "\n";

// Verifies the generated JSON against the expected output.
void CheckJSONIsStillTheSame(const Value& value) {
  // Serialize back the output.
  std::string serialized_json;
  JSONStringValueSerializer str_serializer(&serialized_json);
  str_serializer.set_pretty_print(true);
  ASSERT_TRUE(str_serializer.Serialize(value));
  // Unify line endings between platforms.
  ReplaceSubstringsAfterOffset(&serialized_json, 0, kWinLineEnds,
                               kLinuxLineEnds);
  // Now compare the input with the output.
  ASSERT_EQ(kProperJSON, serialized_json);
}

void ValidateJsonList(const std::string& json) {
  std::optional<Value> value = JSONReader::Read(json);
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  ASSERT_EQ(1U, list->size());
  const Value& elt = (*list)[0];
  ASSERT_TRUE(elt.is_int());
  ASSERT_EQ(1, elt.GetInt());
}

// Test proper JSON deserialization from string is working.
TEST(JSONValueDeserializerTest, ReadProperJSONFromString) {
  // Try to deserialize it through the serializer.
  JSONStringValueDeserializer str_deserializer(kProperJSON);

  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      str_deserializer.Deserialize(&error_code, &error_message);
  ASSERT_TRUE(value);
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(error_message.empty());
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test proper JSON deserialization from a std::string_view substring.
TEST(JSONValueDeserializerTest, ReadProperJSONFromStringPiece) {
  // Create a std::string_view for the substring of kProperJSONPadded that
  // matches kProperJSON.
  std::string_view proper_json(kProperJSONPadded);
  proper_json = proper_json.substr(5, proper_json.length() - 10);
  JSONStringValueDeserializer str_deserializer(proper_json);

  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      str_deserializer.Deserialize(&error_code, &error_message);
  ASSERT_TRUE(value);
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(error_message.empty());
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test that trialing commas are only properly deserialized from string when
// the proper flag for that is set.
TEST(JSONValueDeserializerTest, ReadJSONWithTrailingCommasFromString) {
  // Try to deserialize it through the serializer.
  JSONStringValueDeserializer str_deserializer(kProperJSONWithCommas);

  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      str_deserializer.Deserialize(&error_code, &error_message);
  ASSERT_FALSE(value);
  ASSERT_NE(0, error_code);
  ASSERT_FALSE(error_message.empty());
  // Repeat with commas allowed. The Deserialize call shouldn't change the
  // value of error_code. To test that, we first set it to a nonsense value
  // (-789) and ASSERT_EQ that it remains that nonsense value.
  error_code = -789;
  JSONStringValueDeserializer str_deserializer2(kProperJSONWithCommas,
                                                JSON_ALLOW_TRAILING_COMMAS);
  value = str_deserializer2.Deserialize(&error_code, &error_message);
  ASSERT_TRUE(value);
  ASSERT_EQ(-789, error_code);
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test proper JSON deserialization from file is working.
TEST(JSONValueDeserializerTest, ReadProperJSONFromFile) {
  ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());
  // Write it down in the file.
  FilePath temp_file(tempdir.GetPath().AppendASCII("test.json"));
  ASSERT_TRUE(WriteFile(temp_file, kProperJSON));

  // Try to deserialize it through the serializer.
  JSONFileValueDeserializer file_deserializer(temp_file);

  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      file_deserializer.Deserialize(&error_code, &error_message);
  ASSERT_TRUE(value);
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(error_message.empty());
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test that trialing commas are only properly deserialized from file when
// the proper flag for that is set.
TEST(JSONValueDeserializerTest, ReadJSONWithCommasFromFile) {
  ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());
  // Write it down in the file.
  FilePath temp_file(tempdir.GetPath().AppendASCII("test.json"));
  ASSERT_TRUE(WriteFile(temp_file, kProperJSONWithCommas));

  // Try to deserialize it through the serializer.
  JSONFileValueDeserializer file_deserializer(temp_file);
  // This must fail without the proper flag.
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      file_deserializer.Deserialize(&error_code, &error_message);
  ASSERT_FALSE(value);
  ASSERT_NE(0, error_code);
  ASSERT_FALSE(error_message.empty());
  // Repeat with commas allowed. The Deserialize call shouldn't change the
  // value of error_code. To test that, we first set it to a nonsense value
  // (-789) and ASSERT_EQ that it remains that nonsense value.
  error_code = -789;
  JSONFileValueDeserializer file_deserializer2(temp_file,
                                               JSON_ALLOW_TRAILING_COMMAS);
  value = file_deserializer2.Deserialize(&error_code, &error_message);
  ASSERT_TRUE(value);
  ASSERT_EQ(-789, error_code);
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

TEST(JSONValueDeserializerTest, AllowTrailingComma) {
  static const char kTestWithCommas[] = "{\"key\": [true,],}";
  static const char kTestNoCommas[] = "{\"key\": [true]}";

  JSONStringValueDeserializer deserializer(kTestWithCommas,
                                           JSON_ALLOW_TRAILING_COMMAS);
  JSONStringValueDeserializer deserializer_expected(kTestNoCommas);
  std::unique_ptr<Value> root = deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root);
  std::unique_ptr<Value> root_expected;
  root_expected = deserializer_expected.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root_expected);
  ASSERT_EQ(*root, *root_expected);
}

TEST(JSONValueSerializerTest, Roundtrip) {
  static const char kOriginalSerialization[] =
      "{\"bool\":true,\"double\":3.14,\"int\":42,\"list\":[1,2],\"null\":null}";
  JSONStringValueDeserializer deserializer(kOriginalSerialization);
  std::unique_ptr<Value> root = deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root);
  const Value::Dict* root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);

  const Value* null_value = root_dict->Find("null");
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->is_none());

  ASSERT_TRUE(root_dict->FindBool("bool").value());
  ASSERT_EQ(42, root_dict->FindInt("int").value());
  ASSERT_DOUBLE_EQ(3.14, root_dict->FindDouble("double").value());

  std::string test_serialization;
  JSONStringValueSerializer mutable_serializer(&test_serialization);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  ASSERT_EQ(kOriginalSerialization, test_serialization);

  mutable_serializer.set_pretty_print(true);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  // JSON output uses a different newline style on Windows than on other
  // platforms.
#if BUILDFLAG(IS_WIN)
#define JSON_NEWLINE "\r\n"
#else
#define JSON_NEWLINE "\n"
#endif
  const std::string pretty_serialization =
      "{" JSON_NEWLINE "   \"bool\": true," JSON_NEWLINE
      "   \"double\": 3.14," JSON_NEWLINE "   \"int\": 42," JSON_NEWLINE
      "   \"list\": [ 1, 2 ]," JSON_NEWLINE "   \"null\": null" JSON_NEWLINE
      "}" JSON_NEWLINE;
#undef JSON_NEWLINE
  ASSERT_EQ(pretty_serialization, test_serialization);
}

TEST(JSONValueSerializerTest, StringEscape) {
  std::u16string all_chars;
  for (int i = 1; i < 256; ++i) {
    all_chars += static_cast<char16_t>(i);
  }
  // Generated in in Firefox using the following js (with an extra backslash for
  // double quote):
  // var s = '';
  // for (var i = 1; i < 256; ++i) { s += String.fromCharCode(i); }
  // uneval(s).replace(/\\/g, "\\\\");
  std::string all_chars_expected =
      "\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000B\\f\\r"
      "\\u000E\\u000F\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017"
      "\\u0018\\u0019\\u001A\\u001B\\u001C\\u001D\\u001E\\u001F !\\\"#$%&'()*+,"
      "-./0123456789:;\\u003C=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcde"
      "fghijklmnopqrstuvwxyz{|}~\x7F\xC2\x80\xC2\x81\xC2\x82\xC2\x83\xC2\x84"
      "\xC2\x85\xC2\x86\xC2\x87\xC2\x88\xC2\x89\xC2\x8A\xC2\x8B\xC2\x8C\xC2\x8D"
      "\xC2\x8E\xC2\x8F\xC2\x90\xC2\x91\xC2\x92\xC2\x93\xC2\x94\xC2\x95\xC2\x96"
      "\xC2\x97\xC2\x98\xC2\x99\xC2\x9A\xC2\x9B\xC2\x9C\xC2\x9D\xC2\x9E\xC2\x9F"
      "\xC2\xA0\xC2\xA1\xC2\xA2\xC2\xA3\xC2\xA4\xC2\xA5\xC2\xA6\xC2\xA7\xC2\xA8"
      "\xC2\xA9\xC2\xAA\xC2\xAB\xC2\xAC\xC2\xAD\xC2\xAE\xC2\xAF\xC2\xB0\xC2\xB1"
      "\xC2\xB2\xC2\xB3\xC2\xB4\xC2\xB5\xC2\xB6\xC2\xB7\xC2\xB8\xC2\xB9\xC2\xBA"
      "\xC2\xBB\xC2\xBC\xC2\xBD\xC2\xBE\xC2\xBF\xC3\x80\xC3\x81\xC3\x82\xC3\x83"
      "\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xC3\x8C"
      "\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x90\xC3\x91\xC3\x92\xC3\x93\xC3\x94\xC3\x95"
      "\xC3\x96\xC3\x97\xC3\x98\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C\xC3\x9D\xC3\x9E"
      "\xC3\x9F\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7"
      "\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB0"
      "\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xB7\xC3\xB8\xC3\xB9"
      "\xC3\xBA\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBE\xC3\xBF";

  std::string expected_output =
      "{\"all_chars\":\"" + all_chars_expected + "\"}";
  // Test JSONWriter interface
  std::string output_js;
  Value::Dict valueRoot;
  valueRoot.Set("all_chars", all_chars);
  JSONWriter::Write(valueRoot, &output_js);
  ASSERT_EQ(expected_output, output_js);

  // Test JSONValueSerializer interface (uses JSONWriter).
  JSONStringValueSerializer serializer(&output_js);
  ASSERT_TRUE(serializer.Serialize(valueRoot));
  ASSERT_EQ(expected_output, output_js);
}

TEST(JSONValueSerializerTest, UnicodeStrings) {
  // unicode string json -> escaped ascii text
  Value::Dict root;
  std::u16string test(u"\x7F51\x9875");
  root.Set("web", test);

  static const char kExpected[] = "{\"web\":\"\xE7\xBD\x91\xE9\xA1\xB5\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(kExpected, actual);

  // escaped ascii text -> json
  JSONStringValueDeserializer deserializer(kExpected);
  std::unique_ptr<Value> deserial_root =
      deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(deserial_root);
  const Value::Dict* deserial_root_dict = deserial_root->GetIfDict();
  const std::string* web_value = deserial_root_dict->FindString("web");
  ASSERT_TRUE(web_value);
  ASSERT_EQ("\xE7\xBD\x91\xE9\xA1\xB5", *web_value);
}

TEST(JSONValueSerializerTest, HexStrings) {
  // hex string json -> escaped ascii text
  Value::Dict root;
  std::u16string test(u"\x01\x02");
  root.Set("test", test);

  static const char kExpected[] = "{\"test\":\"\\u0001\\u0002\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(kExpected, actual);

  // escaped ascii text -> json
  JSONStringValueDeserializer deserializer(kExpected);
  std::unique_ptr<Value> deserial_root =
      deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(deserial_root);
  Value::Dict* deserial_root_dict = deserial_root->GetIfDict();
  const std::string* test_value = deserial_root_dict->FindString("test");
  ASSERT_TRUE(test_value);
  ASSERT_EQ("\u0001\u0002", *test_value);

  // Test converting escaped regular chars
  static const char kEscapedChars[] = "{\"test\":\"\\u0067\\u006f\"}";
  JSONStringValueDeserializer deserializer2(kEscapedChars);
  deserial_root = deserializer2.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(deserial_root);
  deserial_root_dict = deserial_root->GetIfDict();
  test_value = deserial_root_dict->FindString("test");
  ASSERT_TRUE(test_value);
  ASSERT_EQ("go", *test_value);
}

TEST(JSONValueSerializerTest, JSONReaderComments) {
  ValidateJsonList("[ // 2, 3, ignore me ] \n1 ]");
  ValidateJsonList("[ /* 2, \n3, ignore me ]*/ \n1 ]");
  ValidateJsonList("//header\n[ // 2, \n// 3, \n1 ]// footer");
  ValidateJsonList("/*\n[ // 2, \n// 3, \n1 ]*/[1]");
  ValidateJsonList("[ 1 /* one */ ] /* end */");
  ValidateJsonList("[ 1 //// ,2\r\n ]");

  // It's ok to have a comment in a string.
  std::optional<Value> value = JSONReader::Read("[\"// ok\\n /* foo */ \"]");
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  ASSERT_EQ(1U, list->size());
  const Value& elt = (*list)[0];
  ASSERT_TRUE(elt.is_string());
  ASSERT_EQ("// ok\n /* foo */ ", elt.GetString());

  // You can't nest comments.
  ASSERT_FALSE(JSONReader::Read("/* /* inner */ outer */ [ 1 ]"));

  // Not a open comment token.
  ASSERT_FALSE(JSONReader::Read("/ * * / [1]"));
}

class JSONFileValueSerializerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  ScopedTempDir temp_dir_;
};

TEST_F(JSONFileValueSerializerTest, Roundtrip) {
  FilePath original_file_path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &original_file_path));
  original_file_path = original_file_path.AppendASCII("serializer_test.json");

  ASSERT_TRUE(PathExists(original_file_path));

  JSONFileValueDeserializer deserializer(original_file_path);
  std::unique_ptr<Value> root = deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root);
  const Value::Dict* root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);

  const Value* null_value = root_dict->Find("null");
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->is_none());

  ASSERT_TRUE(root_dict->FindBool("bool").value());
  ASSERT_EQ(42, root_dict->FindInt("int").value());

  const std::string* string_value = root_dict->FindString("string");
  ASSERT_TRUE(string_value);
  ASSERT_EQ("hello", *string_value);

  // Now try writing.
  const FilePath written_file_path =
      temp_dir_.GetPath().AppendASCII("test_output.js");

  ASSERT_FALSE(PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root_dict));
  ASSERT_TRUE(PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(TextContentsEqual(original_file_path, written_file_path));
  EXPECT_TRUE(DeleteFile(written_file_path));
}

TEST_F(JSONFileValueSerializerTest, RoundtripNested) {
  FilePath original_file_path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &original_file_path));
  original_file_path =
      original_file_path.AppendASCII("serializer_nested_test.json");

  ASSERT_TRUE(PathExists(original_file_path));

  JSONFileValueDeserializer deserializer(original_file_path);
  std::unique_ptr<Value> root = deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root);

  // Now try writing.
  FilePath written_file_path =
      temp_dir_.GetPath().AppendASCII("test_output.json");

  ASSERT_FALSE(PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(TextContentsEqual(original_file_path, written_file_path));
  EXPECT_TRUE(DeleteFile(written_file_path));
}

TEST_F(JSONFileValueSerializerTest, NoWhitespace) {
  FilePath source_file_path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &source_file_path));
  source_file_path =
      source_file_path.AppendASCII("serializer_test_nowhitespace.json");
  ASSERT_TRUE(PathExists(source_file_path));
  JSONFileValueDeserializer deserializer(source_file_path);
  std::unique_ptr<Value> root = deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(root);
}

}  // namespace

}  // namespace base

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace autofill {
namespace test {
namespace {

// Only run these tests on Linux because there are issues with other platforms.
// Testing on one platform gives enough confidence.
#if defined(OS_LINUX)

using base::JSONWriter;
using base::Value;

using RequestResponsePair =
    std::pair<AutofillQueryContents, AutofillQueryResponseContents>;

constexpr char kTestHTTPResponseHeader[] = "Fake HTTP Response Header";
constexpr char kHTTPBodySep[] = "\r\n\r\n";

struct LightField {
  uint32_t signature;
  uint32_t prediction;
};

struct LightForm {
  uint64_t signature;
  std::vector<LightField> fields;
};

RequestResponsePair MakeQueryRequestResponsePair(
    const std::vector<LightForm>& forms) {
  AutofillQueryContents query;
  query.set_client_version("Chrome Test");
  AutofillQueryResponseContents query_response;
  for (const auto& form : forms) {
    auto* added_form = query.add_form();
    added_form->set_signature(form.signature);
    for (const auto& field : form.fields) {
      added_form->add_field()->set_signature(field.signature);
      query_response.add_field()->set_overall_type_prediction(field.prediction);
    }
  }
  return RequestResponsePair({std::move(query), std::move(query_response)});
}

// Makes Query request canonical URL. Will set "q" query parameter if |query| is
// not empty.
bool MakeQueryRequestURL(const base::Optional<AutofillQueryContents>& query,
                         std::string* request_url) {
  constexpr base::StringPiece base_url =
      "https://clients1.google.com/tbproxy/af/query";
  // Add Query proto content to "q" parameter if non-empty.
  if (query.has_value()) {
    std::string encoded_query;
    std::string serialized_query;
    if (!(*query).SerializeToString(&serialized_query)) {
      VLOG(1) << "could not serialize Query proto";
      return false;
    }
    base::Base64Encode(serialized_query, &encoded_query);
    *request_url = base::StrCat({base_url, "?q=", encoded_query});
    return true;
  }

  *request_url = base_url.as_string();
  return true;
}

// Make HTTP request header given |url|.
inline std::string MakeRequestHeader(base::StringPiece url) {
  return base::StrCat({"GET ", url, " ", "HTTP/1.1"});
}

// Makes string value for "SerializedRequest" json node that contains HTTP
// request content.
bool MakeSerializedRequest(const AutofillQueryContents& query,
                           RequestType type,
                           std::string* serialized_request,
                           std::string* request_url) {
  // Make body and query content for URL depending on the |type|.
  std::string body;
  base::Optional<AutofillQueryContents> query_for_url;
  if (type == RequestType::kLegacyQueryProtoGET) {
    query_for_url = std::move(query);
  } else {
    query.SerializeToString(&body);
    query_for_url = base::nullopt;
  }

  // Make header according to query content for URL.
  std::string url;
  if (!MakeQueryRequestURL(query_for_url, &url))
    return false;
  *request_url = url;
  std::string header = MakeRequestHeader(url);

  // Fill HTTP text.
  std::string http_text =
      base::JoinString(std::vector<std::string>{header, body}, kHTTPBodySep);
  base::Base64Encode(http_text, serialized_request);
  return true;
}

std::string MakeSerializedResponse(
    const AutofillQueryResponseContents& query_response) {
  std::string serialized_query;
  query_response.SerializeToString(&serialized_query);
  std::string compressed_query;
  compression::GzipCompress(serialized_query, &compressed_query);
  // TODO(vincb): Put a real header here.
  std::string http_text = base::JoinString(
      std::vector<std::string>{kTestHTTPResponseHeader, compressed_query},
      kHTTPBodySep);
  std::string encoded_http_text;
  base::Base64Encode(http_text, &encoded_http_text);
  return encoded_http_text;
}

// Write json node to file in text format.
bool WriteJSONNode(const base::FilePath& file_path, const base::Value& node) {
  std::string json_text;
  JSONWriter::WriteWithOptions(node, JSONWriter::Options::OPTIONS_PRETTY_PRINT,
                               &json_text);

  std::string compressed_json_text;
  if (!compression::GzipCompress(json_text, &compressed_json_text)) {
    VLOG(1) << "Cannot compress json to gzip.";
    return false;
  }

  if (base::WriteFile(file_path, compressed_json_text.data(),
                      compressed_json_text.size()) == -1) {
    VLOG(1) << "Could not write json at file: " << file_path;
    return false;
  }
  return true;
}

// Write cache to file in json text format.
bool WriteJSON(const base::FilePath& file_path,
               const std::vector<RequestResponsePair>& request_response_pairs,
               RequestType request_type = RequestType::kLegacyQueryProtoPOST) {
  // Make json list node that contains all query requests.
  base::Value::DictStorage urls_dict;
  for (const auto& request_response_pair : request_response_pairs) {
    Value::DictStorage request_response_node;
    std::string serialized_request;
    std::string url;
    if (!MakeSerializedRequest(request_response_pair.first, request_type,
                               &serialized_request, &url)) {
      return false;
    }

    request_response_node["SerializedRequest"] =
        std::make_unique<Value>(std::move(serialized_request));
    request_response_node["SerializedResponse"] = std::make_unique<Value>(
        MakeSerializedResponse(request_response_pair.second));
    // Populate json dict node that contains Autofill Server requests per URL.
    if (urls_dict.find(url) == urls_dict.end())
      urls_dict[url] = std::make_unique<Value>(Value::ListStorage());
    urls_dict[url]->Append(Value(std::move(request_response_node)));
  }

  // Make json dict node that contains requests per domain.
  base::Value::DictStorage domains_dict;
  domains_dict["clients1.google.com"] =
      std::make_unique<Value>(std::move(urls_dict));

  // Make json root dict.
  base::Value::DictStorage root_dict;
  root_dict["Requests"] = std::make_unique<Value>(std::move(domains_dict));

  // Write content to JSON file.
  return WriteJSONNode(file_path, Value(std::move(root_dict)));
}

// TODO(vincb): Add extra death tests.
TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoDomainNode) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // JSON structure is not right.
  const std::string invalid_json = "{\"NoDomainNode\": \"invalid_field\"}";

  // Write json to file.
  ASSERT_TRUE(
      base::WriteFile(file_path, invalid_json.data(), invalid_json.size()) > -1)
      << "there was an error when writing content to json file: " << file_path;

  // Crash since json content is invalid.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord),
      ".*");
}

TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoQueryNodesAndFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Crash since there are no Query nodes and set to fail on empty.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                              ServerCacheReplayer::kOptionFailOnEmpty),
      ".*");
}

// Test suite for GET Query death test.
class AutofillCacheReplayerGETQueryDeathTest
    : public testing::TestWithParam<std::string> {};

TEST_P(AutofillCacheReplayerGETQueryDeathTest,
       ServerCacheReplayerConstructor_CrashesWhenInvalidRequestURLForGETQuery) {
  // Parameterized death test for populating cache when keys that are obtained
  // from the URL's "q" query parameter are invalid.

  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make JSON content.

  // Make json list node that contains the problematic query request.
  Value::DictStorage request_response_node;
  // Put some textual content for HTTP request. Content does not matter because
  // the Query content will be parsed from the URL that corresponds to the
  // dictionary key.
  request_response_node["SerializedRequest"] = std::make_unique<Value>(
      "GET https://clients1.google.com/tbproxy/af/query?q=1234 "
      "HTTP/1.1\r\n\r\n");
  request_response_node["SerializedResponse"] = std::make_unique<Value>(
      MakeSerializedResponse(AutofillQueryResponseContents()));
  // Populate json dict node that contains Autofill Server requests per URL.
  base::Value::DictStorage urls_dict;
  // The "q" parameter in the URL cannot be parsed to a proto because paraneter
  // value is in invalid format.
  std::string invalid_request_url = GetParam();
  urls_dict[invalid_request_url] =
      std::make_unique<Value>(Value::ListStorage());
  urls_dict[invalid_request_url]->Append(
      Value(std::move(request_response_node)));

  // Make json dict node that contains requests per domain.
  base::Value::DictStorage domains_dict;
  domains_dict["clients1.google.com"] =
      std::make_unique<Value>(std::move(urls_dict));
  // Make json root dict.
  base::Value::DictStorage root_dict;
  root_dict["Requests"] = std::make_unique<Value>(std::move(domains_dict));
  // Write content to JSON file.
  ASSERT_TRUE(WriteJSONNode(file_path, Value(std::move(root_dict))));

  // Make death assertion.

  // Crash since request cannot be parsed to a proto.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord),
      ".*");
}

INSTANTIATE_TEST_SUITE_P(
    GetQueryParameterizedDeathTest,
    AutofillCacheReplayerGETQueryDeathTest,
    testing::Values(  // Can be base-64 decoded, but not parsed to proto.
        "https://clients1.google.com/tbproxy/af/query?q=1234",
        // Cannot be base-64 decoded.
        "https://clients1.google.com/tbproxy/af/query?q=^^^"));

TEST(AutofillCacheReplayerTest,
     CanUseReplayerWhenNoCacheContentWithNotFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Should not crash even if no cache because kOptionFailOnEmpty is not
  // flipped.
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     (ServerCacheReplayer::kOptionFailOnEmpty & 0));

  // Should be able to read cache, which will give nothing.
  std::string http_text;
  AutofillQueryContents query_with_no_match;
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(query_with_no_match, &http_text));
}

// Test suite for Query response retrieval test.
class AutofillCacheReplayerGetResponseForQueryTest
    : public testing::TestWithParam<RequestType> {};

TEST_P(AutofillCacheReplayerGetResponseForQueryTest,
       FillsResponseWhenNoErrors) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json.
  RequestType request_type = GetParam();
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs, request_type));

  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we can get cached response.
  std::string http_text_response;
  ASSERT_TRUE(cache_replayer.GetResponseForQuery(
      request_response_pairs[0].first, &http_text_response));
  AutofillQueryResponseContents response_from_cache;
  ASSERT_TRUE(response_from_cache.ParseFromString(
      SplitHTTP(http_text_response).second));
}

INSTANTIATE_TEST_SUITE_P(GetResponseForQueryParameterizeTest,
                         AutofillCacheReplayerGetResponseForQueryTest,
                         testing::Values(
                             // Read Query content from URL "q" param.
                             RequestType::kLegacyQueryProtoGET,
                             // Read Query content from HTTP body.
                             RequestType::kLegacyQueryProtoPOST));

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNullptr) {
  ServerCacheReplayer cache_replayer(ServerCache{{}});
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(AutofillQueryContents(), nullptr));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNoKeyMatch) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we get false when there is no cache for the query.
  std::string http_text;
  AutofillQueryContents query_with_no_match;
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(query_with_no_match, &http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesFalseWhenDecompressFailsBecauseInvalidHTTP) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillQueryContents query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQueryRequest(query_request_for_key);

  const char invalid_http[] = "Dumb Nonsense That Doesn't Have a HTTP Header";
  ServerCacheReplayer cache_replayer(ServerCache{{key, invalid_http}});

  // Verify if we get false when invalid HTTP response to decompress.
  std::string response_http_text;
  EXPECT_FALSE(cache_replayer.GetResponseForQuery(query_request_for_key,
                                                  &response_http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesTrueWhenDecompressSucceededBecauseEmptyBody) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillQueryContents query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQueryRequest(query_request_for_key);

  const char http_without_body[] = "Test HTTP Header\r\n\r\n";
  ServerCacheReplayer cache_replayer(ServerCache{{key, http_without_body}});

  // Verify if we get true when no HTTP body.
  std::string response_http_text;
  EXPECT_TRUE(cache_replayer.GetResponseForQuery(query_request_for_key,
                                                 &response_http_text));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseForLargeKey) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  std::vector<RequestResponsePair> unmatched_existing_keys;
  std::vector<RequestResponsePair> unmatched_different_keys;
  {
    LightForm form_to_add1;
    form_to_add1.signature = 1111;
    form_to_add1.fields = {LightField{1111, 1}, LightField{1112, 31},
                           LightField{1113, 33}};
    LightForm form_to_add2;
    form_to_add2.signature = 2222;
    form_to_add2.fields = {LightField{2221, 2}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add1, form_to_add2}));

    LightForm form_to_add3;
    form_to_add3.signature = 3333;
    form_to_add3.fields = {LightField{3331, 3}};
    LightForm form_to_add4;
    form_to_add4.signature = 4444;
    form_to_add4.fields = {LightField{4441, 4}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add3, form_to_add4}));

    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add1}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add2}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add3}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add4}));

    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add1, form_to_add3}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add2, form_to_add4}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add3, form_to_add2}));
    unmatched_existing_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add4, form_to_add1}));

    LightForm form_to_add5;
    form_to_add5.signature = 5555;
    form_to_add5.fields = {LightField{5551, 42}};
    unmatched_different_keys.push_back(
        MakeQueryRequestResponsePair({form_to_add5}));
  }

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  std::string http_text;

  // First, check the exact same key combos we sent properly respond
  EXPECT_TRUE(cache_replayer.GetResponseForQuery(
      request_response_pairs[0].first, &http_text));
  EXPECT_TRUE(cache_replayer.GetResponseForQuery(
      request_response_pairs[1].first, &http_text));
  // And, inexistent combos - verify they don't
  for (auto query_key : unmatched_existing_keys) {
    EXPECT_FALSE(
        cache_replayer.GetResponseForQuery(query_key.first, &http_text));
  }
  EXPECT_FALSE(cache_replayer.GetResponseForQuery(
      unmatched_different_keys[0].first, &http_text));

  // Now, load the same thing into the cache replayer with
  // ServerCacheReplayer::kOptionSplitRequestsByForm set and expect matches
  // for all combos
  ServerCacheReplayer form_split_cache_replayer(
      file_path, ServerCacheReplayer::kOptionSplitRequestsByForm);
  EXPECT_TRUE(form_split_cache_replayer.GetResponseForQuery(
      request_response_pairs[0].first, &http_text));
  EXPECT_TRUE(form_split_cache_replayer.GetResponseForQuery(
      request_response_pairs[1].first, &http_text));
  for (auto query_key : unmatched_existing_keys) {
    EXPECT_TRUE(form_split_cache_replayer.GetResponseForQuery(query_key.first,
                                                              &http_text));
  }
  EXPECT_FALSE(form_split_cache_replayer.GetResponseForQuery(
      unmatched_different_keys[0].first, &http_text));
}
#endif  // if defined(OS_LINUX)

}  // namespace
}  // namespace test
}  // namespace autofill

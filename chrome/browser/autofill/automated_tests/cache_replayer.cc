// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/protobuf/src/google/protobuf/stubs/status.h"
#include "third_party/protobuf/src/google/protobuf/stubs/statusor.h"
#include "third_party/zlib/google/compression_utils.h"

// TODO(crbug/977571): Change returned bool for ::Status.

namespace autofill {
namespace test {

using base::JSONParserOptions;
using base::JSONReader;

namespace {

using google::protobuf::util::Status;
using google::protobuf::util::StatusOr;

constexpr char kHTTPBodySep[] = "\r\n\r\n";

// Makes an internal error that carries an error message.
Status MakeInternalError(const std::string& error_message) {
  return Status(google::protobuf::util::error::INTERNAL, error_message);
}

// Container that represents a JSON node that contains a list of
// request/response pairs sharing the same URL.
struct QueryNode {
  // Query URL.
  std::string url;
  // Value node with requests mapped with |url|.
  const base::Value* node = nullptr;
};

// Gets a hexadecimal representation of a string.
std::string GetHexString(const std::string& input) {
  std::string output("0x");
  for (auto byte : input) {
    base::StringAppendF(&output, "%02x", static_cast<unsigned char>(byte));
  }
  return output;
}

// Makes HTTP request from a header and body
std::string MakeHTTPTextFromSplit(const std::string& header,
                                  const std::string& body) {
  return base::JoinString({header, body}, kHTTPBodySep);
}

// Determines whether replayer should fail if there is an invalid json record.
bool FailOnError(int options) {
  return static_cast<bool>(options &
                           ServerCacheReplayer::kOptionFailOnInvalidJsonRecord);
}

// Determines whether replayer should fail if there is nothing to fill the cache
// with.
bool FailOnEmpty(int options) {
  return static_cast<bool>(options & ServerCacheReplayer::kOptionFailOnEmpty);
}

// Determines whether replayer should split and cache each form individually.
bool SplitRequestsByForm(int options) {
  return static_cast<bool>(options &
                           ServerCacheReplayer::kOptionSplitRequestsByForm);
}

// Checks the validity of a json value node.
bool CheckNodeValidity(const base::Value* node,
                       const std::string& name,
                       base::Value::Type type) {
  if (node == nullptr) {
    VLOG(1) << "Did not find any " << name << "field in json";
    return false;
  }
  if (node->type() != type) {
    VLOG(1) << "Node value is not of type " << node->type()
            << " when it should be of type " << type;
    return false;
  }
  return true;
}

// Gets the RequestType by guessing from the URL.
RequestType GetRequestTypeFromURL(base::StringPiece url) {
  if (url.find("q=") != std::string::npos) {
    return RequestType::kLegacyQueryProtoGET;
  }
  return RequestType::kLegacyQueryProtoPOST;
}

// Parse AutofillQueryContents or AutofillQueryResponseContents from the given
// |http_text|.
template <class T>
StatusOr<T> ParseProtoContents(const std::string http_text) {
  T proto_contents;
  if (!proto_contents.ParseFromString(http_text)) {
    return MakeInternalError(
        base::StrCat({"could not parse proto:`", proto_contents.GetTypeName(),
                      "` from raw data:`", GetHexString(http_text), "`."}));
  }
  return StatusOr<T>(std::move(proto_contents));
}

// Gets Query request proto content from GET URL.
StatusOr<AutofillQueryContents> GetAutofillQueryContentsFromGETQueryURL(
    const GURL& url) {
  std::string q_value;
  if (!net::GetValueForKeyInQuery(url, "q", &q_value)) {
    // This situation will never happen if check for the presence of "q=" is
    // done before calling this function.
    return MakeInternalError(
        base::StrCat({"could not get any value from \"q\" query parameter in "
                      "Query GET URL: ",
                      url.spec()}));
  }

  // Base64-decode the "q" value.
  std::string decoded_query;
  if (!base::Base64UrlDecode(q_value,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_query)) {
    return MakeInternalError(base::StrCat(
        {"could not base64-decode value of query parameter \"q\" in Query GET "
         "URL: \"",
         q_value, "\""}));
  }
  return ParseProtoContents<AutofillQueryContents>(decoded_query);
}

// Puts all data elements within the request or response body together in a
// single DataElement and returns the buffered content as a string. This ensures
// that all the response body data is utilized.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  network::DataElement unified_data_element;
  unified_data_element.SetToEmptyBytes();
  for (auto it = data_elements->begin(); it != data_elements->end(); ++it) {
    unified_data_element.AppendBytes(it->bytes(), it->length());
  }

  // Using the std::string constructor with length ensures that we don't rely
  // on having a termination character to delimit the string. This is the
  // safest approach.
  return std::string(unified_data_element.bytes(),
                     unified_data_element.length());
}

// Gets Query request proto content from HTTP POST body.
StatusOr<AutofillQueryContents> GetAutofillQueryContentsFromPOSTQuery(
    const network::ResourceRequest& resource_request) {
  return ParseProtoContents<AutofillQueryContents>(
      GetStringFromDataElements(resource_request.request_body->elements()));
}

// Validates, retrieves, and decodes node |node_name| from |request_node| and
// returns it in |decoded_value|. Returns false if unsuccessful.
bool RetrieveValueFromRequestNode(const base::Value& request_node,
                                  const std::string node_name,
                                  std::string* decoded_value) {
  // Get and check field node string.
  std::string serialized_value;
  {
    const base::Value* node = request_node.FindKey(node_name);
    if (!CheckNodeValidity(node, node_name, base::Value::Type::STRING)) {
      VLOG(1) << "Invalid Node in WPR archive";
      return false;
    }
    serialized_value = node->GetString();
  }
  // Decode serialized request string.
  {
    if (!base::Base64Decode(serialized_value, decoded_value)) {
      VLOG(1) << "Could not base64 decode serialized value: "
              << serialized_value;
      return false;
    }
  }
  return true;
}

// Gets AutofillQueryContents from WPR recorded HTTP request body for POST.
StatusOr<AutofillQueryContents> GetAutofillQueryContentsFromRequestNode(
    const base::Value& request_node) {
  std::string decoded_request_text;
  if (!RetrieveValueFromRequestNode(request_node, "SerializedRequest",
                                    &decoded_request_text)) {
    return MakeInternalError(
        "Unable to retrieve serialized request from WPR request_node");
  }
  return ParseProtoContents<AutofillQueryContents>(
      SplitHTTP(decoded_request_text).second);
}

// Gets AutofillQueryResponseContents from WPR recorded HTTP response body.
// Also populates and returns the split |response_header_text|.
StatusOr<AutofillQueryResponseContents>
GetAutofillQueryResponseContentsFromRequestNode(
    const base::Value& request_node,
    std::string* response_header_text) {
  std::string compressed_response_text;
  if (!RetrieveValueFromRequestNode(request_node, "SerializedResponse",
                                    &compressed_response_text)) {
    return MakeInternalError(
        "Unable to retrieve serialized request from WPR request_node");
  }
  auto http_pair = SplitHTTP(compressed_response_text);
  std::string decompressed_body;
  if (!compression::GzipUncompress(http_pair.second, &decompressed_body)) {
    return MakeInternalError(
        base::StrCat({"Could not gzip decompress HTTP response: ",
                      GetHexString(http_pair.second)}));
  }

  // Eventual response needs header information, so lift that as well.
  *response_header_text = http_pair.first;

  return ParseProtoContents<AutofillQueryResponseContents>(decompressed_body);
}

// Fills |cache_to_fill| with the keys from a single |query_request| and
// |query_response| pair. Loops through each form in request and creates an
// individual response of just the associated fields for that request. Uses
// |response_header_text| to build and store well-formed and backwards
// compatible http text in the cache.
bool FillFormSplitCache(const AutofillQueryContents& query_request,
                        const std::string& response_header_text,
                        const AutofillQueryResponseContents& query_response,
                        ServerCache* cache_to_fill) {
  VLOG(2) << "Full Request Key is:" << GetKeyFromQueryRequest(query_request);
  VLOG(2) << "Matching keys from Query request proto:\n" << query_request;
  VLOG(2) << "To field types from Query response proto:\n" << query_response;
  auto current_field = query_response.field().begin();
  for (const auto& form : query_request.form()) {
    std::string key = base::NumberToString(form.signature());
    // If already stored a respones for this key, then just advance the
    // current_field by that offset and continue.
    if (base::Contains((*cache_to_fill), key)) {
      VLOG(2) << "Already added key: " << key;
      current_field += form.field_size();
      continue;
    }
    // Grab fields for this form from overall response and add to unique form
    // object.
    AutofillQueryResponseContents individual_form_response;
    for (int i = 0; i < form.field_size(); i++) {
      if (current_field >= query_response.field().end()) {
        VLOG(1) << "Reached end of query_response fields prematurely!";
        return false;
      }
      individual_form_response.add_field()->CopyFrom(*current_field);
      ++current_field;
    }

    // Compress that form response to a string and gzip it.
    std::string serialized_response;
    if (!individual_form_response.SerializeToString(&serialized_response)) {
      VLOG(1) << "Unable to serialize the new response for key! " << key;
      continue;
    }
    std::string compressed_response_body;
    if (!compression::GzipCompress(serialized_response,
                                   &compressed_response_body)) {
      VLOG(1) << "Unable to compress the new response for key! " << key;
      continue;
    }
    // Final http text is header_text concatenated with a compressed body.
    std::string http_text =
        MakeHTTPTextFromSplit(response_header_text, compressed_response_body);

    VLOG(1) << "Adding key:" << key
            << "\nAnd response:" << individual_form_response;
    (*cache_to_fill)[key] = std::move(http_text);
  }
  return true;
}

// Populates |cache_to_fill| with content from |query_node| that contains a
// list of single request node that share the same URL field (e.g.,
// https://clients1.google.com/tbproxy/af/query) in the WPR capture json cache.
// Returns Status with message when there is an error when parsing the requests
// and OPTION_FAIL_ON_INVALID_JSON is flipped in |options|. Returns status ok
// regardless of errors if OPTION_FAIL_ON_INVALID_JSON is not flipped in
// |options| where bad nodes will be skipped. Keeps a log trace whenever there
// is an error even if OPTION_FAIL_ON_INVALID_JSON is not flipped. Uses only the
// form combinations seen in recorded session if OPTION_SPLIT_REQUESTS_BY_FORM
// is false, fill cache with individual form keys (and expect
// ServerCacheReplayer to be able to split incoming request by key and stitch
// results together).
ServerCacheReplayer::Status PopulateCacheFromQueryNode(
    const QueryNode& query_node,
    int options,
    ServerCache* cache_to_fill) {
  bool fail_on_error = FailOnError(options);
  bool split_requests_by_form = SplitRequestsByForm(options);
  for (const base::Value& request : query_node.node->GetList()) {
    // Get AutofillQueryContents from request.
    bool is_post_request = GetRequestTypeFromURL(query_node.url) ==
                           RequestType::kLegacyQueryProtoPOST;
    StatusOr<AutofillQueryContents> query_request_statusor =
        is_post_request
            ? GetAutofillQueryContentsFromRequestNode(request)
            : GetAutofillQueryContentsFromGETQueryURL(GURL(query_node.url));
    // Only proceed if successfully parse the query request proto, else drop to
    // failure space.
    if (query_request_statusor.ok()) {
      VLOG(2) << "Getting key from Query request proto:\n "
              << query_request_statusor.ValueOrDie();
      std::string key =
          GetKeyFromQueryRequest(query_request_statusor.ValueOrDie());
      bool is_single_form_request =
          query_request_statusor.ValueOrDie().form_size() == 1;
      // Switch to store forms as individuals or only in the groupings that they
      // were sent on recording. If only a single form in request then can use
      // old behavior still and skip decompression and combination steps.
      if (!split_requests_by_form || is_single_form_request) {
        std::string compressed_response_text;
        if (RetrieveValueFromRequestNode(request, "SerializedResponse",
                                         &compressed_response_text)) {
          (*cache_to_fill)[key] = std::move(compressed_response_text);
          VLOG(1) << "Cached response content for key: " << key;
          continue;
        }
      } else {
        // Get AutofillQueryResponseContents and response header text.
        std::string response_header_text;
        StatusOr<AutofillQueryResponseContents> query_response_statusor =
            GetAutofillQueryResponseContentsFromRequestNode(
                request, &response_header_text);
        if (!query_response_statusor.ok()) {
          VLOG(1) << "Unable to get AutofillQueryResponseContents from WPR node"
                  << "SerializedResponse for request:" << key;
        } else {
          // We have a proper request and a proper response, we can populate for
          // each form in the AutofillQueryContents.
          if (FillFormSplitCache(
                  query_request_statusor.ValueOrDie(), response_header_text,
                  query_response_statusor.ValueOrDie(), cache_to_fill)) {
            continue;
          }
        }
      }
    }
    // If we've fallen to this level, something went bad with adding the request
    // node. If fail_on_error is set then abort, else log and try the next one.
    constexpr base::StringPiece status_msg =
        "could not cache query node content";
    if (fail_on_error) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadNode, status_msg.as_string()};
    } else {
      // Keep a trace when not set to fail on bad node.
      VLOG(1) << status_msg;
    }
  }
  return ServerCacheReplayer::Status{ServerCacheReplayer::StatusCode::kOk, ""};
}

// TODO(crbug/958125): Add the possibility to retrieve nodes with different
// Query URLs.
// Finds the Autofill server Query node in dictionary node. Gives nullptr if
// cannot find the node or |domain_dict| is invalid. The |domain_dict| has to
// outlive any usage of the returned value node pointers.
std::vector<QueryNode> FindAutofillQueryNodesInDomainDict(
    const base::Value& domain_dict) {
  if (!domain_dict.is_dict()) {
    return {};
  }
  std::vector<QueryNode> nodes;
  for (const auto& pair : domain_dict.DictItems()) {
    if (pair.first.find("https://clients1.google.com/tbproxy/af/query") !=
        std::string::npos) {
      nodes.push_back(QueryNode{pair.first, &pair.second});
    }
  }
  return nodes;
}

// Populates the cache mapping request keys to their corresponding compressed
// response.
ServerCacheReplayer::Status PopulateCacheFromJSONFile(
    const base::FilePath& json_file_path,
    int options,
    ServerCache* cache_to_fill) {
  // Read json file.
  std::string json_text;
  {
    if (!base::ReadFileToString(json_file_path, &json_text)) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          "Could not read json file: "};
    }
  }

  // Decompress the json text from gzip.
  std::string decompressed_json_text;
  if (!compression::GzipUncompress(json_text, &decompressed_json_text)) {
    return ServerCacheReplayer::Status{
        ServerCacheReplayer::StatusCode::kBadRead,
        "Could not gzip decompress json in file: "};
  }

  // Parse json text content to json value node.
  base::Value root_node;
  {
    JSONReader::ValueWithError value_with_error =
        JSONReader().ReadAndReturnValueWithError(
            decompressed_json_text, JSONParserOptions::JSON_PARSE_RFC);
    if (value_with_error.error_code !=
        JSONReader::JsonParseError::JSON_NO_ERROR) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          base::StrCat({"Could not load cache from json file ",
                        "because: ", value_with_error.error_message})};
    }
    if (value_with_error.value == base::nullopt) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          "JSON Reader could not give any node object from json file"};
    }
    root_node = std::move(value_with_error.value.value());
  }

  {
    const char* const domain = "clients1.google.com";
    const base::Value* domain_node = root_node.FindPath({"Requests", domain});
    if (domain_node == nullptr) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kEmpty,
          base::StrCat({"there were no nodes with autofill query content in "
                        "domain node \"",
                        domain, "\""})};
    }
    std::vector<QueryNode> query_nodes =
        FindAutofillQueryNodesInDomainDict(*domain_node);

    // Fill cache with the content of each Query node. There are 3 possible
    // situations: (1) there is a single Query node that contains POST requests
    // that share the same URL, (2) there is one Query node per GET request
    // where each Query node only contains one request, and (3) a mix of (1) and
    // (2). Exit early with false whenever there is an error parsing a node.
    for (auto query_node : query_nodes) {
      if (!CheckNodeValidity(query_node.node,
                             "Requests->clients1.google.com->clients1.google."
                             "com/tbproxy/af/query*",
                             base::Value::Type::LIST)) {
        return ServerCacheReplayer::Status{
            ServerCacheReplayer::StatusCode::kBadNode,
            "could not read node content for node with URL " + query_node.url};
      }

      // Populate cache from Query node content.
      auto status =
          PopulateCacheFromQueryNode(query_node, options, cache_to_fill);
      if (!status.Ok())
        return status;
      VLOG(1) << "Filled cache with " << cache_to_fill->size()
              << " requests for Query node with URL: " << query_node.url;
    }
  }

  // Return error iff there are no Query nodes and replayer is set to fail on
  // empty.
  if (cache_to_fill->empty() && FailOnEmpty(options)) {
    return ServerCacheReplayer::Status{
        ServerCacheReplayer::StatusCode::kEmpty,
        "there were no nodes with autofill query content for autofill server "
        "domains in JSON"};
  }

  return ServerCacheReplayer::Status{ServerCacheReplayer::StatusCode::kOk, ""};
}

}  // namespace

// Decompressed HTTP response read from WPR capture file. Will set
// |decompressed_http| to "" and return false if there is an error.
bool ServerCacheReplayer::RetrieveAndDecompressStoredHTTP(
    const std::string& key,
    std::string* decompressed_http) const {
  // Safe to use at() here since we looked for key's presence and there is no
  // mutation done when there is concurrency.
  const std::string& http_text = const_cache_.at(key);

  auto header_and_body = SplitHTTP(http_text);
  if (header_and_body.first == "") {
    *decompressed_http = "";
    VLOG(1) << "No header found in supposed HTTP text: " << http_text;
    return false;
  }
  // Look if there is a body to decompress, if not just return HTTP text as is.
  if (header_and_body.second == "") {
    *decompressed_http = http_text;
    VLOG(1) << "There is no HTTP body to decompress: " << http_text;
    return true;
  }
  // TODO(crbug.com/945925): Add compression format detection, return an
  // error if not supported format.
  // Decompress the body.
  std::string decompressed_body;
  if (!compression::GzipUncompress(header_and_body.second,
                                   &decompressed_body)) {
    VLOG(1) << "Could not gzip decompress HTTP response: "
            << GetHexString(header_and_body.second);
    return false;
  }
  // Rebuild the response HTTP text by using the new decompressed body.
  *decompressed_http =
      MakeHTTPTextFromSplit(header_and_body.first, decompressed_body);
  return true;
}

// Gives a pair that contains the HTTP text split in 2, where the first
// element is the HTTP head and the second element is the HTTP body.
std::pair<std::string, std::string> SplitHTTP(const std::string& http_text) {
  const size_t split_index = http_text.find(kHTTPBodySep);
  if (split_index != std::string::npos) {
    const size_t sep_length = std::string(kHTTPBodySep).size();
    std::string head = http_text.substr(0, split_index);
    std::string body =
        http_text.substr(split_index + sep_length, std::string::npos);
    return std::make_pair(std::move(head), std::move(body));
  }
  return std::make_pair("", "");
}

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/autofill_download_manager.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryContents& query) {
  out << "client_version: " << query.client_version();
  for (const auto& form : query.form()) {
    out << "\nForm\n signature: " << form.signature();
    for (const auto& field : form.field()) {
      out << "\n Field\n  signature: " << field.signature();
      if (!field.name().empty())
        out << "\n  name: " << field.name();
      if (!field.type().empty())
        out << "\n  type: " << field.type();
    }
  }
  return out;
}

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/form_structure.cc
std::ostream& operator<<(
    std::ostream& out,
    const autofill::AutofillQueryResponseContents& response) {
  for (const auto& field : response.field()) {
    out << "\nautofill_type: " << field.overall_type_prediction();
  }
  return out;
}

// Gets a key for cache lookup from a query request.
std::string GetKeyFromQueryRequest(const AutofillQueryContents& query_request) {
  std::vector<std::string> form_ids;
  for (const auto& form : query_request.form()) {
    form_ids.push_back(base::NumberToString(form.signature()));
  }
  std::sort(form_ids.begin(), form_ids.end());
  return base::JoinString(form_ids, "_");
}

ServerCacheReplayer::~ServerCacheReplayer() {}

ServerCacheReplayer::ServerCacheReplayer(const base::FilePath& json_file_path,
                                         int options)
    : split_requests_by_form_(SplitRequestsByForm(options)) {
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash than being in an inconsistent state
  // when the cache could not be properly populated from the JSON file.
  ServerCacheReplayer::Status status =
      PopulateCacheFromJSONFile(json_file_path, options, &cache_);
  CHECK(status.Ok()) << status.message;
}

ServerCacheReplayer::ServerCacheReplayer(ServerCache server_cache,
                                         bool split_requests_by_form)
    : cache_(std::move(server_cache)),
      split_requests_by_form_(split_requests_by_form) {}

bool ServerCacheReplayer::GetResponseForQuery(
    const AutofillQueryContents& query,
    std::string* http_text) const {
  if (http_text == nullptr) {
    VLOG(1) << "Cannot fill |http_text| because null";
    return false;
  }
  std::string combined_key = GetKeyFromQueryRequest(query);

  if (base::Contains(const_cache_, combined_key)) {
    VLOG(1) << "Retrieving response for " << combined_key;
    std::string decompressed_http_response;
    if (!RetrieveAndDecompressStoredHTTP(combined_key,
                                         &decompressed_http_response)) {
      return false;
    }
    *http_text = decompressed_http_response;
    return true;
  }
  // If we didn't find a single-form match and we're not splitting requests by
  // form, we failed to find a response for this query.
  if (!split_requests_by_form_) {
    VLOG(1) << "Did not match any response for " << combined_key;
    return false;
  }

  AutofillQueryResponseContents combined_form_response;
  std::string response_header_text;
  bool first_loop = true;
  for (const auto& form : query.form()) {
    std::string key = base::NumberToString(form.signature());
    if (!base::Contains(const_cache_, key)) {
      VLOG(2) << "Stubbing in fields for uncached key `" << key << "`.";
      for (int i = 0; i < form.field_size(); i++) {
        AutofillQueryResponseContents::Field* new_field =
            combined_form_response.add_field();
        new_field->set_overall_type_prediction(0);
      }
      continue;
    }
    std::string decompressed_http_response;
    if (!RetrieveAndDecompressStoredHTTP(key, &decompressed_http_response)) {
      return false;
    }
    if (first_loop) {
      response_header_text = SplitHTTP(decompressed_http_response).first;
      first_loop = false;
    }
    StatusOr<AutofillQueryResponseContents> single_form_response =
        ParseProtoContents<AutofillQueryResponseContents>(
            SplitHTTP(decompressed_http_response).second);
    if (!single_form_response.ok()) {
      VLOG(1) << "Unable to parse result contents for key:" << key;
      return false;
    }
    for (auto& field : single_form_response.ValueOrDie().field()) {
      combined_form_response.add_field()->CopyFrom(field);
    }
  }
  // If all we got were stubbed forms, return false as not a single match.
  if (first_loop) {
    VLOG(1) << "Did not match any response for " << combined_key;
    return false;
  }

  std::string compressed_response;
  if (!combined_form_response.SerializeToString(&compressed_response)) {
    VLOG(1) << "Unable to serialize the new response for keys!";
    return false;
  }
  VLOG(1) << "Retrieving stitched response for " << combined_key;
  *http_text = MakeHTTPTextFromSplit(response_header_text, compressed_response);
  return true;
}

ServerUrlLoader::ServerUrlLoader(
    std::unique_ptr<ServerCacheReplayer> cache_replayer)
    : cache_replayer_(std::move(cache_replayer)),
      interceptor_(base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
            return InterceptAutofillRequest(params);
          })) {
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash with a CHECK rather than
  // segfaulting with a stack trace that can be hard to read.
  CHECK(cache_replayer_);
}

ServerUrlLoader::~ServerUrlLoader() {}

bool ServerUrlLoader::InterceptAutofillRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  static const char kDefaultAutofillServerQueryURL[] =
      "https://clients1.google.com/tbproxy/af/query";
  const network::ResourceRequest& resource_request = params->url_request;
  base::StringPiece request_url = resource_request.url.spec();
  // Let all requests that are not autofill queries go to WPR.
  if (request_url.find(kDefaultAutofillServerQueryURL) == std::string::npos) {
    return false;
  }

  // Intercept autofill query and serve back response from cache.
  // Parse HTTP request body to proto.
  VLOG(1) << "Intercepted in-flight request to Autofill Server: "
          << resource_request.url.spec();

  bool is_post_request =
      GetRequestTypeFromURL(request_url) == RequestType::kLegacyQueryProtoPOST;
  // Look if the body has data if it is a POST request.
  if (is_post_request && resource_request.request_body == nullptr) {
    constexpr char kNoBodyHTTPErrorHeaders[] = "HTTP/2.0 400 Bad Request";
    constexpr char kNoBodyHTTPErrorBody[] =
        "there is no body data in the request";
    VLOG(1) << "Served Autofill error response: " << kNoBodyHTTPErrorBody;
    content::URLLoaderInterceptor::WriteResponse(
        std::string(kNoBodyHTTPErrorHeaders), std::string(kNoBodyHTTPErrorBody),
        params->client.get());
    return true;
  }

  StatusOr<AutofillQueryContents> query_request_statusor =
      is_post_request
          ? GetAutofillQueryContentsFromPOSTQuery(resource_request)
          : GetAutofillQueryContentsFromGETQueryURL(resource_request.url);
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash rather than missing the cache
  // because the request content could not be parsed back to a Query request
  // proto, which can be caused by bad data in the request from the browser
  // during capture replay.
  CHECK(query_request_statusor.ok()) << query_request_statusor.status();

  // Get response from cache using query request proto as key.
  std::string http_response;
  if (!cache_replayer_->GetResponseForQuery(query_request_statusor.ValueOrDie(),
                                            &http_response)) {
    // Give back 404 error to the server if there is not match in cache.
    constexpr char kNoKeyMatchHTTPErrorHeaders[] = "HTTP/2.0 404 Not Found";
    constexpr char kNoKeyMatchHTTPErrorBody[] =
        "could not find response matching request";
    VLOG(1) << "Served Autofill error response: " << kNoKeyMatchHTTPErrorBody;
    content::URLLoaderInterceptor::WriteResponse(
        std::string(kNoKeyMatchHTTPErrorHeaders),
        std::string(kNoKeyMatchHTTPErrorBody), params->client.get());
    return true;
  }
  // Give back cache response HTTP content.
  auto http_pair = SplitHTTP(http_response);
  content::URLLoaderInterceptor::WriteResponse(
      http_pair.first, http_pair.second, params->client.get());
  VLOG(1) << "Giving back response from cache";
  return true;
}

}  // namespace test
}  // namespace autofill

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "third_party/zlib/google/compression_utils.h"

namespace autofill::test {

using base::JSONParserOptions;
using base::JSONReader;

namespace {

constexpr char kHTTPBodySep[] = "\r\n\r\n";
constexpr char kApiServerDomain[] = "content-autofill.googleapis.com";
constexpr char kApiServerUrlGetPrefix[] =
    "https://content-autofill.googleapis.com/v1/pages";
constexpr char kApiServerQueryPath[] = "/v1/pages";

template <typename T>
using ErrorOr = base::expected<T, std::string>;

// Container that represents a JSON node that contains a list of
// request/response pairs sharing the same URL.
struct QueryNode {
  // Query URL.
  GURL url;
  // Value node with requests mapped with |url|.
  raw_ptr<const base::Value> node = nullptr;
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

// Checks the type of a json value node.
bool CheckNodeType(const base::Value* node,
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

// Parse AutofillQueryResponse from the given |http_text|.
template <class T>
ErrorOr<T> ParseProtoContents(const std::string& http_text) {
  T proto_contents;
  if (!proto_contents.ParseFromString(http_text)) {
    return base::unexpected(
        base::StrCat({"could not parse proto:`", proto_contents.GetTypeName(),
                      "` from raw data:`", GetHexString(http_text), "`."}));
  }
  return std::move(proto_contents);
}

// Gets base64 encoded query parameter from the URL.
ErrorOr<std::string> GetQueryParameter(const GURL& url) {
  std::string value = url.path();
  if (value.find(kApiServerQueryPath) != 0) {
    // This situation will never happen if check for the query path is
    // done before calling this function.
    return base::unexpected(
        "could not get any value from query path in Query GET URL: " +
        url.spec());
  }
  size_t slash = value.find('/', strlen(kApiServerQueryPath));
  if (slash != std::string::npos) {
    return base::ok(value.substr(slash + 1));
  } else {
    return base::unexpected(
        "could not get any value from query path in Query GET URL: " +
        url.spec());
  }
}

// Returns whether the |url| points to a GET or POST query, or neither.
RequestType GetRequestTypeFromURL(const GURL& url) {
  if (url.host() != kApiServerDomain ||
      url.path().find(kApiServerQueryPath) != 0) {
    return RequestType::kNone;
  }

  std::string path = url.path().substr(strlen(kApiServerQueryPath));
  return path == ":get" || path == ":get/" ? RequestType::kQueryProtoPOST
                                           : RequestType::kQueryProtoGET;
}

// Gets query request protos from GET URL.
ErrorOr<AutofillPageQueryRequest> GetAutofillQueryFromGETQueryURL(
    const GURL& url) {
  ErrorOr<std::string> query_parameter = GetQueryParameter(url);
  if (!query_parameter.has_value())
    return base::unexpected(query_parameter.error());

  // Base64-decode the query value.
  std::string decoded_query;
  if (!base::Base64UrlDecode(query_parameter.value(),
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_query)) {
    return base::unexpected(base::StrCat(
        {"could not base64-decode value of path in Query GET URL: \"",
         *query_parameter, "\""}));
  }
  return ParseProtoContents<AutofillPageQueryRequest>(decoded_query);
}

// Puts all data elements within the request or response body together in a
// single DataElement and returns the buffered content as a string. This ensures
// that all the response body data is utilized.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  std::string result;
  for (const network::DataElement& element : *data_elements) {
    DCHECK_EQ(element.type(), network::DataElement::Tag::kBytes);
    // Provide the length of the bytes explicitly, not to rely on the null
    // termination.
    const auto piece = element.As<network::DataElementBytes>().AsStringPiece();
    result.append(piece);
  }
  return result;
}

// Queries for the Api environment are special in the sense that the actual
// AutofillPageQueryRequest is base64 encoded and wrapped in an
// AutofillPageResourceQueryRequest.
ErrorOr<std::string> PeelAutofillPageResourceQueryRequestWrapper(
    const std::string& text) {
  ErrorOr<AutofillPageResourceQueryRequest> request =
      ParseProtoContents<AutofillPageResourceQueryRequest>(text);
  if (!request.has_value())
    return base::unexpected(request.error());
  std::string encoded_query = request->serialized_request();
  std::string query;
  if (!base::Base64UrlDecode(encoded_query,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &query)) {
    return base::unexpected(base::StrCat(
        {"could not base64-decode serialized body of a POST request: \"",
         encoded_query.c_str(), "\""}));
  }
  return base::ok(std::move(query));
}

// Gets Query request proto content from HTTP POST body.
ErrorOr<AutofillPageQueryRequest> GetAutofillQueryFromPOSTQuery(
    const network::ResourceRequest& resource_request) {
  return PeelAutofillPageResourceQueryRequestWrapper(
             GetStringFromDataElements(
                 resource_request.request_body->elements()))
      .and_then(ParseProtoContents<AutofillPageQueryRequest>);
}

bool IsSingleFormRequest(const AutofillPageQueryRequest& query) {
  return query.forms_size() == 1;
}

// Validates, retrieves, and decodes node |node_name| from |request_node| and
// returns it in |decoded_value|. Returns false if unsuccessful.
bool RetrieveValueFromRequestNode(const base::Value::Dict& request_node,
                                  const std::string& node_name,
                                  std::string* decoded_value) {
  // Get and check field node string.
  std::string serialized_value;
  {
    const base::Value* node = request_node.Find(node_name);
    if (!CheckNodeType(node, node_name, base::Value::Type::STRING)) {
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

// Gets AutofillPageQueryRequest from WPR recorded HTTP request body for POST.
ErrorOr<AutofillPageQueryRequest> GetAutofillQueryFromRequestNode(
    const base::Value::Dict& request_node) {
  std::string decoded_request_text;
  if (!RetrieveValueFromRequestNode(request_node, "SerializedRequest",
                                    &decoded_request_text)) {
    return base::unexpected(
        "Unable to retrieve serialized request from WPR request_node");
  }
  std::string http_text = SplitHTTP(decoded_request_text).second;
  return PeelAutofillPageResourceQueryRequestWrapper(http_text).and_then(
      ParseProtoContents<AutofillPageQueryRequest>);
}

// Gets AutofillQueryResponse from WPR recorded HTTP response body.
// Also populates and returns the split |response_header_text|.
ErrorOr<AutofillQueryResponse> GetAutofillResponseFromRequestNode(
    const base::Value::Dict& request_node,
    std::string* response_header_text) {
  std::string compressed_response_text;
  if (!RetrieveValueFromRequestNode(request_node, "SerializedResponse",
                                    &compressed_response_text)) {
    return base::unexpected(
        "Unable to retrieve serialized request from WPR request_node");
  }
  auto http_pair = SplitHTTP(compressed_response_text);
  std::string decompressed_body;
  if (!compression::GzipUncompress(http_pair.second, &decompressed_body)) {
    return base::unexpected("Could not gzip decompress HTTP response: " +
                            GetHexString(http_pair.second));
  }

  // Eventual response needs header information, so lift that as well.
  *response_header_text = http_pair.first;

  // The Api Environment expects the response to be base64 encoded.
  std::string tmp;
  if (!base::Base64Decode(decompressed_body, &tmp)) {
    return base::unexpected("Unable to base64 decode the body");
  }
  decompressed_body = tmp;

  return ParseProtoContents<AutofillQueryResponse>(decompressed_body);
}

// Fills |cache_to_fill| with the keys from a single |query_request| and
// |query_response| pair. Loops through each form in request and creates an
// individual response of just the associated fields for that request. Uses
// |response_header_text| to build and store well-formed and backwards
// compatible http text in the cache.
bool FillFormSplitCache(const AutofillPageQueryRequest& query_request,
                        const std::string& response_header_text,
                        const AutofillQueryResponse& query_response,
                        ServerCache* cache_to_fill) {
  VLOG(2) << "Full Request Key is:" << GetKeyFromQuery(query_request);
  VLOG(2) << "Matching keys from Query request proto:\n" << query_request;
  VLOG(2) << "To field types from Query response proto:\n" << query_response;
  if (query_request.forms_size() != query_response.form_suggestions_size()) {
    VLOG(1) << "Response did not contain the same number of forms as the query";
    return false;
  }
  for (int i = 0; i < query_request.forms_size(); ++i) {
    const auto& query_form = query_request.forms(i);
    const auto& response_form = query_response.form_suggestions(i);
    std::string key = base::NumberToString(query_form.signature());
    // If already stored a respones for this key, then just advance the
    // current_field by that offset and continue.
    if (base::Contains((*cache_to_fill), key)) {
      VLOG(2) << "Already added key: " << key;
      continue;
    }
    // Grab fields for this form from overall response and add to unique form
    // object.
    AutofillQueryResponse individual_form_response;
    individual_form_response.add_form_suggestions()->CopyFrom(response_form);

    // Compress that form response to a string and gzip it.
    std::string serialized_response;
    if (!individual_form_response.SerializeToString(&serialized_response)) {
      VLOG(1) << "Unable to serialize the new response for key! " << key;
      continue;
    }
    // Chrome expects the response to be base64 encoded.
    std::string serialized_response_base64 =
        base::Base64Encode(serialized_response);
    std::string compressed_response_body;
    if (!compression::GzipCompress(serialized_response_base64,
                                   &compressed_response_body)) {
      VLOG(1) << "Unable to compress the new response for key! " << key;
      continue;
    }
    // Final http text is header_text concatenated with a compressed body.
    std::string http_text =
        MakeHTTPTextFromSplit(response_header_text, compressed_response_body);

    VLOG(2) << "Adding key:" << key
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
    // Get AutofillPageQueryRequest from request.
    bool is_post_request =
        GetRequestTypeFromURL(query_node.url) == RequestType::kQueryProtoPOST;
    ErrorOr<AutofillPageQueryRequest> query_request_statusor =
        is_post_request ? GetAutofillQueryFromRequestNode(request.GetDict())
                        : GetAutofillQueryFromGETQueryURL(GURL(query_node.url));
    // Only proceed if successfully parse the query request proto, else drop to
    // failure space.
    if (query_request_statusor.has_value()) {
      VLOG(2) << "Getting key from Query request proto:\n "
              << query_request_statusor.value();
      std::string key = GetKeyFromQuery(query_request_statusor.value());
      bool is_single_form_request =
          IsSingleFormRequest(query_request_statusor.value());
      // Switch to store forms as individuals or only in the groupings that they
      // were sent on recording. If only a single form in request then can use
      // old behavior still and skip decompression and combination steps.
      if (!split_requests_by_form || is_single_form_request) {
        std::string compressed_response_text;
        if (RetrieveValueFromRequestNode(request.GetDict(),
                                         "SerializedResponse",
                                         &compressed_response_text)) {
          (*cache_to_fill)[key] = compressed_response_text;
          VLOG(2) << "Cached response content for key: " << key;
          continue;
        }
      } else {
        // Get AutofillQueryResponse and response header text.
        std::string response_header_text;
        ErrorOr<AutofillQueryResponse> query_response_statusor =
            GetAutofillResponseFromRequestNode(request.GetDict(),
                                               &response_header_text);
        if (!query_response_statusor.has_value()) {
          VLOG(1) << "Unable to get AutofillQueryResponse from WPR node"
                  << "SerializedResponse for request:" << key;
          continue;
        }
        // We have a proper request and a proper response, we can populate for
        // each form in the AutofillPageQueryRequest.
        if (FillFormSplitCache(
                query_request_statusor.value(), response_header_text,
                query_response_statusor.value(), cache_to_fill)) {
          continue;
        }
      }
    }
    // If we've fallen to this level, something went bad with adding the request
    // node. If fail_on_error is set then abort, else log and try the next one.
    constexpr std::string_view status_msg =
        "could not cache query node content";
    if (fail_on_error) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadNode, std::string(status_msg)};
    } else {
      // Keep a trace when not set to fail on bad node.
      VLOG(1) << status_msg;
    }
  }
  return ServerCacheReplayer::Status{ServerCacheReplayer::StatusCode::kOk, ""};
}

// Finds the Autofill server Query nodes in a dictionary node. The |domain| has
// to outlive any usage of the returned value node pointers.
std::vector<QueryNode> FindQueryNodesInDomainDict(
    const base::Value::Dict& domain,
    const std::string& url_prefix) {
  std::vector<QueryNode> nodes;
  for (auto pair : domain) {
    if (pair.first.find(url_prefix) != std::string::npos) {
      nodes.push_back(QueryNode{GURL(pair.first), &pair.second});
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
    auto value_with_error = JSONReader::ReadAndReturnValueWithError(
        decompressed_json_text, JSONParserOptions::JSON_PARSE_RFC);
    if (!value_with_error.has_value()) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          base::StrCat({"Could not load cache from json file ",
                        "because: ", value_with_error.error().message})};
    }
    root_node = std::move(*value_with_error);
  }

  {
    std::vector<QueryNode> query_nodes;
    const base::Value::Dict* root_node_dict = root_node.GetIfDict();
    if (root_node_dict) {
      const base::Value::Dict* requests = root_node_dict->FindDict("Requests");
      if (requests) {
        const base::Value::Dict* domain_node =
            requests->FindDict(kApiServerDomain);
        if (domain_node) {
          query_nodes =
              FindQueryNodesInDomainDict(*domain_node, kApiServerUrlGetPrefix);
        }
      }
    }

    // Fill cache with the content of each Query node. There are 3 possible
    // situations: (1) there is a single Query node that contains POST requests
    // that share the same URL, (2) there is one Query node per GET request
    // where each Query node only contains one request, and (3) a mix of (1) and
    // (2). Exit early with false whenever there is an error parsing a node.
    for (auto query_node : query_nodes) {
      if (!CheckNodeType(query_node.node,
                         "Requests->content-autofill.googleapis.com->"
                         "content-autofill.googleapis.com/v1/pages:get*",
                         base::Value::Type::LIST)) {
        return ServerCacheReplayer::Status{
            ServerCacheReplayer::StatusCode::kBadNode,
            base::StrCat({"could not read node content for node with URL ",
                          query_node.url.spec()})};
      }

      // Populate cache from Query node content.
      // The template parameters specify the reading and writing format.
      auto status =
          PopulateCacheFromQueryNode(query_node, options, cache_to_fill);
      if (!status.Ok())
        return status;
      VLOG(2) << "Filled cache with " << cache_to_fill->size()
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
bool RetrieveAndDecompressStoredHTTP(const ServerCache& cache,
                                     const std::string& key,
                                     std::string* decompressed_http) {
  // Safe to use at() here since we looked for key's presence and there is no
  // mutation done when there is concurrency.
  const std::string& http_text = cache.at(key);

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
  // TODO(crbug.com/40620146): Add compression format detection, return an
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

// Determines the Autofill Server Behavior from command line parameter.
AutofillServerBehaviorType ParseAutofillServerBehaviorType() {
  std::string autofill_server_option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kAutofillServerBehaviorParam);
  VLOG(1) << "Autofill Server Behavior was:`" << autofill_server_option << "`.";
  if (autofill_server_option.empty() ||
      base::EqualsCaseInsensitiveASCII(autofill_server_option, "SavedCache")) {
    return AutofillServerBehaviorType::kSavedCache;
  } else if (base::EqualsCaseInsensitiveASCII(autofill_server_option,
                                              "ProductionServer")) {
    return AutofillServerBehaviorType::kProductionServer;
  } else if (base::EqualsCaseInsensitiveASCII(autofill_server_option,
                                              "OnlyLocalHeuristics")) {
    return AutofillServerBehaviorType::kOnlyLocalHeuristics;
  } else {
    CHECK(false) << "Unrecognized command line value give for `"
                 << kAutofillServerBehaviorParam << "` argument: `"
                 << autofill_server_option << "`";
    return AutofillServerBehaviorType::kSavedCache;
  }
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
// components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillPageQueryRequest& query) {
  for (const auto& form : query.forms()) {
    out << "\nForm\n signature: " << form.signature();
    for (const auto& field : form.fields()) {
      out << "\n Field\n  signature: " << field.signature();
      if (!field.name().empty())
        out << "\n  name: " << field.name();
      if (!field.control_type().empty())
        out << "\n  control_type: " << field.control_type();
    }
  }
  return out;
}

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/form_structure.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryResponse& response) {
  for (const auto& form : response.form_suggestions()) {
    out << "\nForm";
    for (const auto& field : form.field_suggestions()) {
      out << "\n Field\n  signature: " << field.field_signature();
      for (const auto& prediction : field.predictions())
        out << "\n  prediction: " << prediction.type();
    }
  }
  return out;
}

// Gets a key for cache lookup from a query request.
std::string GetKeyFromQuery(const AutofillPageQueryRequest& query_request) {
  std::vector<std::string> form_ids;
  for (const auto& form : query_request.forms()) {
    form_ids.push_back(base::NumberToString(form.signature()));
  }
  std::sort(form_ids.begin(), form_ids.end());
  return base::JoinString(form_ids, "_");
}

ServerCacheReplayer::~ServerCacheReplayer() = default;

ServerCacheReplayer::ServerCacheReplayer(const base::FilePath& json_file_path,
                                         int options)
    : split_requests_by_form_(SplitRequestsByForm(options)) {
  // If the behavior type is not cache, we can skip setup.
  if (test::ParseAutofillServerBehaviorType() !=
      AutofillServerBehaviorType::kSavedCache)
    return;

  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash than being in an inconsistent
  // state when the cache could not be properly populated from the JSON file.
  ServerCacheReplayer::Status status =
      PopulateCacheFromJSONFile(json_file_path, options, &cache_);
  CHECK(status.Ok()) << status.message;
}

ServerCacheReplayer::ServerCacheReplayer(ServerCache server_cache,
                                         bool split_requests_by_form)
    : cache_(std::move(server_cache)),
      split_requests_by_form_(split_requests_by_form) {}

// Retrieve forms of an api server response.
const ::google::protobuf::RepeatedPtrField<AutofillPageQueryRequest_Form>&
GetFormsRef(const AutofillPageQueryRequest& query) {
  return query.forms();
}

void CreateEmptyResponseForFormQuery(const AutofillPageQueryRequest_Form& form,
                                     AutofillQueryResponse* response) {
  auto* new_form = response->add_form_suggestions();
  for (int i = 0; i < form.fields_size(); i++) {
    auto* new_field = new_form->add_field_suggestions();
    new_field->add_predictions()->set_type(0);
  }
}

void AppendSingleFormResponse(const AutofillQueryResponse& single_form_response,
                              AutofillQueryResponse* response) {
  auto* new_form = response->add_form_suggestions();
  new_form->CopyFrom(single_form_response.form_suggestions(0));
}

bool GetResponseForQuery(const ServerCacheReplayer& cache_replayer,
                         const AutofillPageQueryRequest& query,
                         std::string* http_text) {
  if (http_text == nullptr) {
    VLOG(1) << "Cannot fill |http_text| because null";
    return false;
  }
  const ServerCache& cache = cache_replayer.cache();
  bool split_requests_by_form = cache_replayer.split_requests_by_form();
  std::string combined_key = GetKeyFromQuery(query);

  if (base::Contains(cache, combined_key)) {
    VLOG(1) << "Retrieving response for " << combined_key;
    std::string decompressed_http_response;
    if (!RetrieveAndDecompressStoredHTTP(cache, combined_key,
                                         &decompressed_http_response)) {
      return false;
    }
    *http_text = decompressed_http_response;
    return true;
  }
  // If we didn't find a single-form match and we're not splitting requests by
  // form, we failed to find a response for this query.
  if (!split_requests_by_form) {
    VLOG(1) << "Did not match any response for " << combined_key;
    return false;
  }

  // Assemble a new response from single form requests.
  AutofillQueryResponse combined_form_response;
  std::string response_header_text;
  bool first_loop = true;
  for (const auto& form : GetFormsRef(query)) {
    std::string key = base::NumberToString(form.signature());
    if (!base::Contains(cache, key)) {
      VLOG(2) << "Stubbing in fields for uncached key `" << key << "`.";
      CreateEmptyResponseForFormQuery(form, &combined_form_response);
      continue;
    }
    std::string decompressed_http_response;
    if (!RetrieveAndDecompressStoredHTTP(cache, key,
                                         &decompressed_http_response)) {
      return false;
    }
    if (first_loop) {
      response_header_text = SplitHTTP(decompressed_http_response).first;
      first_loop = false;
    }
    std::string body = SplitHTTP(decompressed_http_response).second;
    // The Api Environment expects the response to be base64 encoded.
    std::string tmp;
    if (!base::Base64Decode(body, &tmp)) {
      VLOG(1) << "Unable to base64 decode contents for key: " << key
              << ", contents: " << body;
      return false;
    }
    body = tmp;

    ErrorOr<AutofillQueryResponse> single_form_response =
        ParseProtoContents<AutofillQueryResponse>(body);
    if (!single_form_response.has_value()) {
      VLOG(1) << "Unable to parse result contents for key:" << key;
      return false;
    }
    AppendSingleFormResponse(single_form_response.value(),
                             &combined_form_response);
  }
  // If all we got were stubbed forms, return false as not a single match.
  if (first_loop) {
    VLOG(1) << "Did not match any response for " << combined_key;
    return false;
  }

  std::string serialized_response;
  if (!combined_form_response.SerializeToString(&serialized_response)) {
    VLOG(1) << "Unable to serialize the new response for keys!";
    return false;
  }
  // The Api Environment expects the response body to be base64 encoded.
  serialized_response = base::Base64Encode(serialized_response);

  VLOG(1) << "Retrieving stitched response for " << combined_key;
  *http_text = MakeHTTPTextFromSplit(response_header_text, serialized_response);
  return true;
}

bool ServerCacheReplayer::GetApiServerResponseForQuery(
    const AutofillPageQueryRequest& query,
    std::string* http_text) const {
  return GetResponseForQuery(*this, query, http_text);
}

ServerUrlLoader::ServerUrlLoader(
    std::unique_ptr<ServerCacheReplayer> cache_replayer)
    : cache_replayer_(std::move(cache_replayer)),
      autofill_server_behavior_type_(ParseAutofillServerBehaviorType()),
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

bool WriteNotFoundResponse(
    content::URLLoaderInterceptor::RequestParams* params) {
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

// Return a 400 Bad Request message to |client|.
void SendBadRequest(network::mojom::URLLoaderClient* client) {
  constexpr char kNoBodyHTTPErrorHeaders[] = "HTTP/2.0 400 Bad Request";
  constexpr char kNoBodyHTTPErrorBody[] =
      "there is no body data in the request";
  VLOG(1) << "Served Autofill error response: " << kNoBodyHTTPErrorBody;
  content::URLLoaderInterceptor::WriteResponse(
      std::string(kNoBodyHTTPErrorHeaders), std::string(kNoBodyHTTPErrorBody),
      client);
}

bool InterceptAutofillRequestHelper(
    const ServerCacheReplayer& cache_replayer,
    content::URLLoaderInterceptor::RequestParams* params) {
  const network::ResourceRequest& resource_request = params->url_request;
  RequestType request_type = GetRequestTypeFromURL(resource_request.url);
  CHECK_NE(request_type, RequestType::kNone);

  // Intercept autofill query and serve back response from cache.
  // Parse HTTP request body to proto.
  VLOG(1) << "Intercepted in-flight request to Autofill Server: "
          << resource_request.url.spec();

  bool is_post_request = (request_type == RequestType::kQueryProtoPOST);
  // Look if the body has data if it is a POST request.
  if (is_post_request && resource_request.request_body == nullptr) {
    SendBadRequest(params->client.get());
    return true;
  }

  ErrorOr<AutofillPageQueryRequest> query_request_statusor =
      is_post_request ? GetAutofillQueryFromPOSTQuery(resource_request)
                      : GetAutofillQueryFromGETQueryURL(resource_request.url);
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash rather than missing the cache
  // because the request content could not be parsed back to a Query request
  // proto, which can be caused by bad data in the request from the browser
  // during capture replay.
  CHECK(query_request_statusor.has_value()) << query_request_statusor.error();

  // Get response from cache using query request proto as key.
  std::string http_response;
  if (!GetResponseForQuery(cache_replayer, query_request_statusor.value(),
                           &http_response)) {
    return WriteNotFoundResponse(params);
  }
  // Give back cache response HTTP content.
  auto http_pair = SplitHTTP(http_response);
  content::URLLoaderInterceptor::WriteResponse(
      http_pair.first, http_pair.second, params->client.get());
  VLOG(2) << "Giving back response from cache";
  return true;
}

bool ServerUrlLoader::InterceptAutofillRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  const network::ResourceRequest& resource_request = params->url_request;
  const GURL& request_url = resource_request.url;
  bool api_query_request = (request_url.host() == kApiServerDomain &&
                            request_url.path().find(kApiServerQueryPath) == 0);
  if (api_query_request) {
    // Check what the set behavior type is.
    //   For Production Server, return false to say don't intercept.
    //   For Only Local Heuristics, write empty server response.
    //   For Saved Cache, continue on and look for a response in the cache.
    switch (autofill_server_behavior_type_) {
      case AutofillServerBehaviorType::kProductionServer:
        return false;
      case AutofillServerBehaviorType::kOnlyLocalHeuristics:
        return WriteNotFoundResponse(params);
      case AutofillServerBehaviorType::kSavedCache:
      default:
        break;
    }
    return InterceptAutofillRequestHelper(*cache_replayer_, params);
  }

  // Let all requests that are not autofill queries go to WPR.
  return false;
}

}  // namespace autofill::test

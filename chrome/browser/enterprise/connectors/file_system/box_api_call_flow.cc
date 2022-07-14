// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_response.h"

#include <string>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_endpoints.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#define PRINT_ERROR(net_error, head)                                \
  (net_error ? base::StringPrintf("net error = %d", net_error)      \
             : base::StringPrintf("response_code = %d\nheader: %s", \
                                  head->headers->response_code(),   \
                                  head->headers->raw_headers().c_str()))

#define LOG_API_FAIL(severity, flow, net_error, head, body)                 \
  DCHECK(net_error || head);                                                \
  const auto error_str = PRINT_ERROR(net_error, head);                      \
  DLOG(severity) << "[BoxApiCallFlow] " << flow << " failed; " << error_str \
                 << ";\nbody: " << (body ? *body : "<null>");

#define LOG_PARSE_FAIL(severity, flow, result)                            \
  DLOG(severity) << "[BoxApiCallFlow] " << flow << "\nJson Parse Error: " \
                 << (!result.has_value() ? result.error().data()          \
                                         : "<no error info>");

#define LOG_PARSE_FAIL_IF(condition, severity, flow, result) \
  if (condition) {                                           \
    LOG_PARSE_FAIL(severity, flow, result);                  \
  }

namespace {

// Create folder at root.
static const char kParentFolderId[] = "0";

std::string ExtractId(const base::Value& entry) {
  DCHECK(entry.is_dict()) << entry;
  const base::Value* id_val = entry.FindPath("id");
  if (!id_val) {
    DLOG(ERROR) << "[BoxApiCallFlow] Can't find id! " << entry;
    return std::string();
  }

  std::string id;
  switch (id_val->type()) {
    case base::Value::Type::STRING:
      id = id_val->GetString();
      break;
    case base::Value::Type::INTEGER:
      id = base::NumberToString(id_val->GetInt());
      break;
    default:
      DLOG(ERROR) << "[BoxApiCallFlow] Invalid id_val type: "
                  << id_val->GetTypeName(id_val->type());
  }
  return id;
}

std::string ExtractParentId(const base::Value& value) {
  std::string id;
  const base::Value* parent = nullptr;
  const base::Value* parent_id = nullptr;

  parent = value.FindPath("parent");

  if (parent)
    parent_id = parent->FindPath("id");
  if (parent_id && parent_id->is_int())
    id = base::NumberToString(parent_id->GetInt());
  else
    DLOG(ERROR) << "[BoxApiCallFlow] Parent ID not found";

  return id;
}

base::Value CreateEmptyDict() {
  return base::Value(base::Value::Type::DICTIONARY);
}

base::Value CreateSingleFieldDict(const std::string& key,
                                  const std::string& value) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(key, value);
  return dict;
}

bool VerifyChunkedUploadParts(const base::Value& parts) {
  DCHECK(parts.is_dict()) << parts;
  DCHECK(parts.FindPath("parts")->is_list()) << parts;
  auto parts_list = parts.FindPath("parts")->GetListDeprecated();
  DCHECK(!parts_list.empty());
  for (auto p = parts_list.begin(); p != parts_list.end(); ++p) {
    DCHECK(p->is_dict()) << parts;
    DCHECK(p->FindPath("part_id")) << parts;
    DCHECK(p->FindPath("offset")) << parts;
    DCHECK(p->FindPath("size")) << parts;
    DCHECK(p->FindPath("sha1")) << parts;
  }
  return true;
}

using Box = enterprise_connectors::BoxApiCallFlow;

bool ExtractEntriesList(const Box::ParseResult& result,
                        base::Value::ConstListView* list) {
  if (!result.has_value()) {
    return false;
  }

  const base::Value* entries = result->GetDict().Find("entries");
  if (!entries || !entries->is_list()) {
    return false;
  }

  CHECK(list);
  *list = entries->GetListDeprecated();
  return true;
}

std::string ExtractUploadedFileId(const Box::ParseResult& result) {
  base::Value::ConstListView list;
  std::string file_id;
  if (ExtractEntriesList(result, &list) && !list.empty()) {
    file_id = ExtractId(list.front());
  }
  LOG_PARSE_FAIL_IF(file_id.empty(), ERROR, "ExtractUploadedFileId", result);
  return file_id;
}

void ProcessUploadSuccessResponse(
    std::unique_ptr<std::string> body,
    base::OnceCallback<void(const std::string&)> callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(
                 [](decltype(callback) cb, Box::ParseResult result) {
                   std::move(cb).Run(ExtractUploadedFileId(result));
                 },
                 std::move(callback)));
}

}  // namespace

namespace enterprise_connectors {

const char kBoxEnterpriseIdFieldName[] = "enterprise.id";
const char kBoxLoginFieldName[] = "login";
const char kBoxNameFieldName[] = "name";

// File size limit according to https://developer.box.com/guides/uploads/:
// - Chucked upload APIs is only supported for file size >= 20 MB;
// - Whole file upload API is only supported for file size <= 50 MB.
const size_t BoxApiCallFlow::kChunkFileUploadMinSize =
    20 * 1024 * 1024;  // 20 MB
const size_t BoxApiCallFlow::kWholeFileUploadMaxSize =
    50 * 1024 * 1024;  // 50 MB

BoxApiCallResponse MakeSuccess(int http_code) {
  DCHECK_GT(http_code, 0);
  return BoxApiCallResponse{true, http_code};
}

BoxApiCallResponse MakeNetworkFailure(int net_code) {
  DCHECK_LT(net_code, 0);
  return BoxApiCallResponse{false, net_code};
}

BoxApiCallResponse MakeApiFailure(int http_code,
                                  std::string box_error_code,
                                  std::string box_request_id) {
  return BoxApiCallResponse{false, http_code, std::move(box_error_code),
                            std::move(box_request_id)};
}

BoxApiCallFlow::BoxApiCallFlow() = default;
BoxApiCallFlow::~BoxApiCallFlow() = default;

GURL BoxApiCallFlow::CreateApiCallUrl() {
  return GURL(kFileSystemBoxEndpointApi);
}

std::string BoxApiCallFlow::CreateApiCallBody() {
  return std::string();
}
std::string BoxApiCallFlow::CreateApiCallBodyContentType() {
  return "application/json";
}

void BoxApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (net_error) {
    DCHECK(net_error < 0);
    LOG_API_FAIL(ERROR, "network", net_error, head, body);
    ProcessFailure(MakeNetworkFailure(net_error));
  } else if (head && head->headers &&
             head->headers->response_code() == net::HTTP_UNAUTHORIZED) {
    ProcessFailure(Response{false, net::HTTP_UNAUTHORIZED});
  } else {
    DCHECK(head);
    DCHECK(head->headers);
    DCHECK(body);
    DCHECK(body->size());
    LOG_API_FAIL(ERROR,
                 "API request " << GetRequestTypeForBody("dummy body") << " to "
                                << CreateApiCallUrl(),
                 net_error, head, body);
    data_decoder::DataDecoder::ParseJsonIsolated(
        *body, base::BindOnce(&BoxApiCallFlow::OnFailureJsonParsed,
                              weak_factory_.GetWeakPtr(),
                              head->headers->response_code()));
  }
}

// API reference: https://developer.box.com/reference/resources/client-error/
void BoxApiCallFlow::OnFailureJsonParsed(int http_code, ParseResult result) {
  base::Value *code = nullptr, *request_id = nullptr;
  auto response = Response{false, http_code};
  if (result.has_value() && (code = result->FindPath("code")) &&
      (request_id = result->FindPath("request_id"))) {
    response =
        MakeApiFailure(http_code, code->GetString(), request_id->GetString());
  }
  LOG_PARSE_FAIL_IF(!code || !request_id, ERROR, "OnFailureJsonParsed", result);
  ProcessFailure(response);
}

// Box API reference:
net::PartialNetworkTrafficAnnotationTag
BoxApiCallFlow::GetNetworkTrafficAnnotationTag() {
  return net::DefinePartialNetworkTrafficAnnotation(
      "file_system_connector_to_box", "oauth2_api_call_flow", R"(
      semantics {
        sender: "Chrome Enterprise File System Connector"
        description:
          "Communication to Box API (https://developer.box.com/reference/) to "
          "upload or download files."
        trigger:
          "A request from the user to download a file when the enterprise admin"
          " has enabled file download redirection."
        data: "Any file that is being downloaded/uploaded by the user."
        destination: OTHER
        destination_other: "Box storage in the cloud."
      }
      policy {
        cookies_allowed: NO
        setting:
          "No settings control."
        policy_exception_justification: "Not implemented yet."
      })");
  // TODO(https://crbug.com/1157959): Add the policy to turn on/off connector.
}

// static
std::string BoxApiCallFlow::FormatSHA1Digest(const std::string& sha_digest) {
  return base::StringPrintf("sha=%s=", sha_digest.c_str());
}

// static
GURL BoxApiCallFlow::MakeUrlToShowFile(const std::string& file_id) {
  return file_id.empty() ? GURL()
                         : GURL("https://app.box.com/file/").Resolve(file_id);
}

// static
GURL BoxApiCallFlow::MakeUrlToShowFolder(const std::string& folder_id) {
  return folder_id.empty()
             ? GURL()
             : GURL("https://app.box.com/folder/").Resolve(folder_id);
}

////////////////////////////////////////////////////////////////////////////////
// GetFileFolder
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/resources/file/
BoxGetFileFolderApiCallFlow::BoxGetFileFolderApiCallFlow(
    TaskCallback callback,
    const std::string& file_id)
    : callback_(std::move(callback)), file_id_(file_id) {}
BoxGetFileFolderApiCallFlow::~BoxGetFileFolderApiCallFlow() = default;

GURL BoxGetFileFolderApiCallFlow::CreateApiCallUrl() {
  std::string path("2.0/files/" + file_id_);
  return BoxApiCallFlow::CreateApiCallUrl().Resolve(path);
}

void BoxGetFileFolderApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  CHECK_EQ(response_code, net::HTTP_OK);

  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(&BoxGetFileFolderApiCallFlow::OnSuccessJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxGetFileFolderApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, std::string());
}

void BoxGetFileFolderApiCallFlow::OnSuccessJsonParsed(ParseResult result) {
  std::string folder_id;

  if (result.has_value())
    folder_id = ExtractParentId(*result);

  std::move(callback_).Run(Response{!folder_id.empty(), net::HTTP_OK},
                           folder_id);
}

////////////////////////////////////////////////////////////////////////////////
// FindUpstreamFolder
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/get-search/#param-200-application/json
BoxFindUpstreamFolderApiCallFlow::BoxFindUpstreamFolderApiCallFlow(
    TaskCallback callback)
    : callback_(std::move(callback)) {}
BoxFindUpstreamFolderApiCallFlow::~BoxFindUpstreamFolderApiCallFlow() = default;

GURL BoxFindUpstreamFolderApiCallFlow::CreateApiCallUrl() {
  std::string path("2.0/search?type=folder&query=ChromeDownloads");
  GURL call_url = BoxApiCallFlow::CreateApiCallUrl().Resolve(path);
  return call_url;
}

void BoxFindUpstreamFolderApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  CHECK_EQ(response_code, net::HTTP_OK);

  data_decoder::DataDecoder::ParseJsonIsolated(
      *body,
      base::BindOnce(&BoxFindUpstreamFolderApiCallFlow::OnSuccessJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void BoxFindUpstreamFolderApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, std::string());
}

void BoxFindUpstreamFolderApiCallFlow::OnSuccessJsonParsed(ParseResult result) {
  base::Value::ConstListView list;
  std::string folder_id;
  bool extracted = ExtractEntriesList(result, &list);
  LOG_PARSE_FAIL_IF(!extracted, ERROR, "FindUpstreamFolder", result);

  if (extracted && !list.empty()) {
    folder_id = ExtractId(list.front());
    extracted = !folder_id.empty();
  }
  std::move(callback_).Run(Response{extracted, net::HTTP_OK}, folder_id);
}

////////////////////////////////////////////////////////////////////////////////
// CreateUpstreamFolder
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference: https://developer.box.com/reference/post-folders/
BoxCreateUpstreamFolderApiCallFlow::BoxCreateUpstreamFolderApiCallFlow(
    TaskCallback callback)
    : callback_(std::move(callback)) {}
BoxCreateUpstreamFolderApiCallFlow::~BoxCreateUpstreamFolderApiCallFlow() =
    default;

GURL BoxCreateUpstreamFolderApiCallFlow::CreateApiCallUrl() {
  return BoxApiCallFlow::CreateApiCallUrl().Resolve("2.0/folders");
}

std::string BoxCreateUpstreamFolderApiCallFlow::CreateApiCallBody() {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetStringKey("name", "ChromeDownloads");
  val.SetKey("parent", CreateSingleFieldDict("id", kParentFolderId));

  std::string body;
  base::JSONWriter::Write(val, &body);
  return body;
}

bool BoxCreateUpstreamFolderApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_CREATED || code == net::HTTP_CONFLICT;
}

void BoxCreateUpstreamFolderApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  data_decoder::DataDecoder::ParseJsonIsolated(
      *body,
      base::BindOnce(&BoxCreateUpstreamFolderApiCallFlow::OnSuccessJsonParsed,
                     weak_factory_.GetWeakPtr(), response_code));
}

void BoxCreateUpstreamFolderApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, std::string());
}

void BoxCreateUpstreamFolderApiCallFlow::OnSuccessJsonParsed(
    int network_response_code,
    ParseResult result) {
  DCHECK(result.has_value());
  if (!result.has_value())
    return OnFailureJsonParsed(network_response_code, std::move(result));

  std::string folder_id;
  absl::optional<base::Value> folder_info_dict;

  if (network_response_code == net::HTTP_CREATED) {
    folder_info_dict = std::move(*result);
  } else {
    // Right after a folder was created with a previous upload, the folder may
    // not be found via BoxFindUpstreamFolderApiCallFlow, therefore BoxUploader
    // tries to create a folder again and gets a conflict. The conflicting
    // folder is included in the response body so can also be extracted to
    // return a folder_id.
    DCHECK_EQ(network_response_code, net::HTTP_CONFLICT);
    std::string* box_error_code = result->FindStringPath("code");
    base::Value* conflict_folders_list =
        result->FindListPath("context_info.conflicts");
    if (box_error_code && *box_error_code == "item_name_in_use" &&
        conflict_folders_list &&
        conflict_folders_list->GetListDeprecated().size() > 0) {
      folder_info_dict = absl::make_optional<base::Value>(
          conflict_folders_list->GetListDeprecated()[0].Clone());
    }
  }

  if (!folder_info_dict.has_value())
    return OnFailureJsonParsed(network_response_code, std::move(result));

  folder_id = ExtractId(*folder_info_dict);
  LOG_PARSE_FAIL_IF(folder_id.empty(), ERROR, "CreateUpstreamFolder", result);
  std::move(callback_).Run(Response{!folder_id.empty(), network_response_code},
                           folder_id);
  return;
}

////////////////////////////////////////////////////////////////////////////////
// GetCurrentUser
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/get-users-me/
BoxGetCurrentUserApiCallFlow::BoxGetCurrentUserApiCallFlow(
    base::OnceCallback<void(Response, base::Value)> callback)
    : callback_(std::move(callback)) {}
BoxGetCurrentUserApiCallFlow::~BoxGetCurrentUserApiCallFlow() = default;

GURL BoxGetCurrentUserApiCallFlow::CreateApiCallUrl() {
  return BoxApiCallFlow::CreateApiCallUrl().Resolve(
      "2.0/users/me?fields=enterprise,login,name");
}

bool BoxGetCurrentUserApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_OK;
}

void BoxGetCurrentUserApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  DCHECK_EQ(response_code, net::HTTP_OK);
  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(&BoxGetCurrentUserApiCallFlow::OnJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxGetCurrentUserApiCallFlow::OnJsonParsed(ParseResult result) {
  if (!result.has_value()) {
    LOG_PARSE_FAIL(ERROR, "GetCurrentUser", result);
    std::move(callback_).Run(Response{false, net::HTTP_OK}, CreateEmptyDict());
    return;
  }
  if (!result->is_dict() ||
      !result->FindStringPath(kBoxEnterpriseIdFieldName) ||
      !result->FindStringPath(kBoxLoginFieldName) ||
      !result->FindStringPath(kBoxNameFieldName)) {
    LOG(ERROR)
        << "[BoxApiCallFlow] GetCurrentUser succeeded but "
           "response does not include all of enterprise_id, login, and name: "
        << *result;
    std::move(callback_).Run(Response{false, net::HTTP_OK}, CreateEmptyDict());
    return;
  }
  std::move(callback_).Run(Response{true, net::HTTP_OK}, std::move(*result));
}

void BoxGetCurrentUserApiCallFlow::ProcessFailure(Response response) {
  DCHECK(!response.success);
  std::move(callback_).Run(response, CreateEmptyDict());
}

////////////////////////////////////////////////////////////////////////////////
// PreflightCheck
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/options-files-content/
BoxPreflightCheckApiCallFlow::BoxPreflightCheckApiCallFlow(
    TaskCallback callback,
    const base::FilePath& target_file_name,
    const std::string& folder_id)
    : callback_(std::move(callback)),
      target_file_name_(target_file_name),
      folder_id_(folder_id) {}
BoxPreflightCheckApiCallFlow::~BoxPreflightCheckApiCallFlow() = default;

GURL BoxPreflightCheckApiCallFlow::CreateApiCallUrl() {
  return BoxApiCallFlow::CreateApiCallUrl().Resolve("2.0/files/content");
}

std::string BoxPreflightCheckApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  CHECK(!body.empty());
  return "OPTIONS";
}

std::string BoxPreflightCheckApiCallFlow::CreateApiCallBody() {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetStringKey("name", target_file_name_.MaybeAsASCII());
  val.SetKey("parent", CreateSingleFieldDict("id", folder_id_));

  std::string body;
  base::JSONWriter::Write(val, &body);
  return body;
}

bool BoxPreflightCheckApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_OK;
}

void BoxPreflightCheckApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  CHECK_EQ(response_code, net::HTTP_OK);
  std::move(callback_).Run(MakeSuccess(response_code));
}

void BoxPreflightCheckApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response);
}

////////////////////////////////////////////////////////////////////////////////
// WholeFileUpload
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/post-files-content/

BoxWholeFileUploadApiCallFlow::BoxWholeFileUploadApiCallFlow(
    TaskCallback callback,
    const std::string& folder_id,
    const std::string& mime_type,
    const base::FilePath& target_file_name,
    const base::FilePath& local_file_path)
    : folder_id_(folder_id),
      mime_type_(mime_type),
      target_file_name_(target_file_name),
      local_file_path_(local_file_path),
      multipart_boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)) {
  DCHECK(!mime_type_.empty())
      << "No MIME type for download, will not send content-type header";
  DCHECK(!folder_id_.empty());
  DCHECK(!target_file_name_.empty());
  DCHECK(!local_file_path_.empty());
  DCHECK(!multipart_boundary_.empty());
}

BoxWholeFileUploadApiCallFlow::~BoxWholeFileUploadApiCallFlow() = default;

void BoxWholeFileUploadApiCallFlow::Start(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  // Forward the arguments via PostReadFileTask() then OnFileRead() into
  // OAuth2CallFlow::Start().
  PostReadFileTask(url_loader_factory, access_token);
}

void BoxWholeFileUploadApiCallFlow::PostReadFileTask(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  auto read_file_task = base::BindOnce(&BoxWholeFileUploadApiCallFlow::ReadFile,
                                       local_file_path_);
  auto read_file_reply = base::BindOnce(
      &BoxWholeFileUploadApiCallFlow::OnFileRead, weak_factory_.GetWeakPtr(),
      url_loader_factory, access_token);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      std::move(read_file_task), std::move(read_file_reply));
}

// static
absl::optional<std::string> BoxWholeFileUploadApiCallFlow::ReadFile(
    const base::FilePath& path) {
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(path, &file_content,
                                         kWholeFileUploadMaxSize)) {
    DLOG(ERROR) << "Cannot read file " << path;
    return absl::nullopt;
  }
  DCHECK_LE(file_content.size(), kWholeFileUploadMaxSize);
  return absl::optional<std::string>(std::move(file_content));
}

void BoxWholeFileUploadApiCallFlow::OnFileRead(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    absl::optional<std::string> file_content) {
  if (!file_content) {
    DLOG(ERROR) << "[BoxApiCallFlow] WholeFileUpload read file failed";
    // TODO(https://crbug.com/1165972): error handling
    ProcessFailure(Response{false, 0});
    return;
  }
  DCHECK_LE(file_content_.size(), kWholeFileUploadMaxSize);
  file_content_ = std::move(*file_content);

  // Continue to the original call flow after file has been read.
  OAuth2ApiCallFlow::Start(url_loader_factory, access_token);
}

GURL BoxWholeFileUploadApiCallFlow::CreateApiCallUrl() {
  return GURL(kFileSystemBoxEndpointWholeFileUpload);
}

std::string BoxWholeFileUploadApiCallFlow::CreateApiCallBody() {
  base::Value attr(base::Value::Type::DICTIONARY);
  attr.SetStringKey("name", target_file_name_.MaybeAsASCII());
  attr.SetKey("parent", CreateSingleFieldDict("id", folder_id_));

  std::string attr_json;
  base::JSONWriter::Write(attr, &attr_json);

  std::string body;
  net::AddMultipartValueForUpload("attributes", attr_json, multipart_boundary_,
                                  "application/json", &body);

  net::AddMultipartValueForUploadWithFileName(
      "file", target_file_name_.MaybeAsASCII(), file_content_,
      multipart_boundary_, mime_type_, &body);
  net::AddMultipartFinalDelimiterForUpload(multipart_boundary_, &body);

  return body;
}

// Header format for multipart/form-data reference:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
std::string BoxWholeFileUploadApiCallFlow::CreateApiCallBodyContentType() {
  std::string content_type = "multipart/form-data; boundary=";
  content_type.append(multipart_boundary_);
  return content_type;
}

bool BoxWholeFileUploadApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_CREATED;
}

void BoxWholeFileUploadApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  DCHECK_EQ(head->headers->response_code(), net::HTTP_CREATED);
  ProcessUploadSuccessResponse(
      std::move(body),
      base::BindOnce(std::move(callback_), MakeSuccess(net::HTTP_CREATED)));
}

void BoxWholeFileUploadApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, std::string());
}

void BoxWholeFileUploadApiCallFlow::SetFileReadForTesting(std::string content) {
  file_content_ = std::move(content);
}

////////////////////////////////////////////////////////////////////////////////
// ChunkedUpload: CreateUploadSession
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/post-files-upload-sessions/

BoxCreateUploadSessionApiCallFlow::BoxCreateUploadSessionApiCallFlow(
    TaskCallback callback,
    const std::string& folder_id,
    const size_t file_size,
    const base::FilePath& file_name)
    : callback_(std::move(callback)),
      folder_id_(folder_id),
      file_size_(file_size),
      file_name_(file_name) {}

BoxCreateUploadSessionApiCallFlow::~BoxCreateUploadSessionApiCallFlow() =
    default;

GURL BoxCreateUploadSessionApiCallFlow::CreateApiCallUrl() {
  return GURL("https://upload.box.com/api/2.0/files/upload_sessions");
}

std::string BoxCreateUploadSessionApiCallFlow::CreateApiCallBody() {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetStringKey("folder_id", folder_id_);
  val.SetIntKey("file_size", file_size_);  // TODO(https://crbug.com/1187152)
  val.SetStringKey("file_name", file_name_.MaybeAsASCII());

  bool file_big_enough = file_size_ > kChunkFileUploadMinSize;
  CHECK(file_big_enough) << file_size_;

  std::string body;
  base::JSONWriter::Write(val, &body);
  return body;
}

bool BoxCreateUploadSessionApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_CREATED;
}

void BoxCreateUploadSessionApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  CHECK_EQ(response_code, net::HTTP_CREATED);

  data_decoder::DataDecoder::ParseJsonIsolated(
      *body,
      base::BindOnce(&BoxCreateUploadSessionApiCallFlow::OnSuccessJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void BoxCreateUploadSessionApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, CreateEmptyDict(), 0);
}

void BoxCreateUploadSessionApiCallFlow::OnSuccessJsonParsed(
    ParseResult result) {
  LOG_PARSE_FAIL_IF(!result.has_value(), ERROR, "CreateUploadSession", result);

  const auto http_code = net::HTTP_CREATED;
  base::Value *endpoints = nullptr, *part_size = nullptr;
  if (result.has_value() && (part_size = result->FindPath("part_size")) &&
      (endpoints = result->FindPath("session_endpoints")) &&
      endpoints->FindPath("upload_part") && endpoints->FindPath("commit") &&
      endpoints->FindPath("abort")) {
    std::move(callback_).Run(MakeSuccess(http_code), std::move(*endpoints),
                             part_size->GetInt());
    return;
  }
  LOG_PARSE_FAIL_IF(!result.has_value(), ERROR, "CreateUploadSession", result);
  ProcessFailure(MakeApiFailure(http_code, "bad_response", "parse_fail"));
}

////////////////////////////////////////////////////////////////////////////////
// ChunkedUpload: Base
////////////////////////////////////////////////////////////////////////////////
BoxChunkedUploadBaseApiCallFlow::BoxChunkedUploadBaseApiCallFlow(
    const GURL endpoint)
    : endpoint_(endpoint) {
  DCHECK(endpoint_.is_valid());
}

GURL BoxChunkedUploadBaseApiCallFlow::CreateApiCallUrl() {
  return endpoint_;
}

////////////////////////////////////////////////////////////////////////////////
// ChunkedUpload: PartFileUpload
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/put-files-upload-sessions-id/

BoxPartFileUploadApiCallFlow::BoxPartFileUploadApiCallFlow(
    TaskCallback callback,
    const std::string& session_endpoint,
    const std::string& file_part_content,
    const size_t byte_from,
    const size_t byte_to,
    const size_t byte_total)
    : BoxChunkedUploadBaseApiCallFlow(GURL(session_endpoint)),
      callback_(std::move(callback)),
      part_content_(file_part_content),
      content_range_(base::StringPrintf("bytes %zu-%zu/%zu",
                                        byte_from,
                                        byte_to,
                                        byte_total)) {}

BoxPartFileUploadApiCallFlow::~BoxPartFileUploadApiCallFlow() = default;

// static
std::string BoxPartFileUploadApiCallFlow::CreateFileDigest(
    const std::string& content) {
  // Box API requires the digest to be SHA1 and Base64 encoded.
  std::string sha_encoded;
  base::Base64Encode(base::SHA1HashString(content), &sha_encoded);
  return FormatSHA1Digest(sha_encoded);
}

net::HttpRequestHeaders BoxPartFileUploadApiCallFlow::CreateApiCallHeaders() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("content-range", content_range_);
  headers.SetHeader("digest", CreateFileDigest(part_content_));
  return headers;
}

std::string BoxPartFileUploadApiCallFlow::CreateApiCallBody() {
  return part_content_;
}

std::string BoxPartFileUploadApiCallFlow::CreateApiCallBodyContentType() {
  return "application/octet-stream";
}

std::string BoxPartFileUploadApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  CHECK(!body.empty()) << content_range_;
  return "PUT";
}

bool BoxPartFileUploadApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_OK;
}

void BoxPartFileUploadApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  DCHECK(body);
  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(&BoxPartFileUploadApiCallFlow::OnSuccessJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxPartFileUploadApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, base::Value());
}

void BoxPartFileUploadApiCallFlow::OnSuccessJsonParsed(ParseResult result) {
  const auto http_code = net::HTTP_OK;
  if (!result.has_value()) {
    LOG_PARSE_FAIL(ERROR, "PartFileUpload", result);
    ProcessFailure(MakeApiFailure(http_code, "bad_response", "parse_fail"));
    return;
  }

  base::Value* part = result->FindPath("part");
  if (!part) {
    DLOG(ERROR) << "[BoxApiCallFlow] No info for uploaded part";
    ProcessFailure(MakeApiFailure(http_code, "bad_response", "parse_fail"));
  } else {
    std::move(callback_).Run(MakeSuccess(http_code), std::move(*part));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ChunkedUpload: AbortUploadSession
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/delete-files-upload-sessions-id/
BoxAbortUploadSessionApiCallFlow::BoxAbortUploadSessionApiCallFlow(
    TaskCallback callback,
    const std::string& session_endpoint)
    : BoxChunkedUploadBaseApiCallFlow(GURL(session_endpoint)),
      callback_(std::move(callback)) {}

BoxAbortUploadSessionApiCallFlow::~BoxAbortUploadSessionApiCallFlow() = default;

bool BoxAbortUploadSessionApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_NO_CONTENT;
}

std::string BoxAbortUploadSessionApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  return "DELETE";
}

void BoxAbortUploadSessionApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  DCHECK(!body || body->empty()) << "Expecting an empty body.";
  std::move(callback_).Run(MakeSuccess(net::HTTP_NO_CONTENT));
}

void BoxAbortUploadSessionApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response);
}

////////////////////////////////////////////////////////////////////////////////
// ChunkedUpload: CommitUploadSession
////////////////////////////////////////////////////////////////////////////////
// BoxApiCallFlow interface.
// API reference:
// https://developer.box.com/reference/post-files-upload-sessions-id-commit/
BoxCommitUploadSessionApiCallFlow::BoxCommitUploadSessionApiCallFlow(
    TaskCallback callback,
    const std::string& session_endpoint,
    const base::Value& parts,
    const std::string digest)
    : BoxChunkedUploadBaseApiCallFlow(GURL(session_endpoint)),
      callback_(std::move(callback)),
      sha_digest_(digest),
      upload_session_parts_(parts.Clone()) {}

BoxCommitUploadSessionApiCallFlow::~BoxCommitUploadSessionApiCallFlow() =
    default;

net::HttpRequestHeaders
BoxCommitUploadSessionApiCallFlow::CreateApiCallHeaders() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("digest", FormatSHA1Digest(sha_digest_));
  return headers;
}

std::string BoxCommitUploadSessionApiCallFlow::CreateApiCallBody() {
  base::Value parts(base::Value::Type::DICTIONARY);
  parts.SetKey("parts", std::move(upload_session_parts_));
  DCHECK(VerifyChunkedUploadParts(parts));
  std::string body;
  base::JSONWriter::Write(parts, &body);
  return body;
}

bool BoxCommitUploadSessionApiCallFlow::IsExpectedSuccessCode(int code) const {
  return code == net::HTTP_CREATED || code == net::HTTP_ACCEPTED;
}

void BoxCommitUploadSessionApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();

  if (response_code == net::HTTP_CREATED) {
    DCHECK(body);
    DCHECK(body->size());
    auto created_cb = base::BindOnce(
        std::move(callback_), MakeSuccess(response_code), base::TimeDelta());
    return ProcessUploadSuccessResponse(std::move(body), std::move(created_cb));
  }

  bool success = false;
  base::TimeDelta retry_after;
  if (response_code == net::HTTP_ACCEPTED) {
    std::string retry_string;
    success =
        head->headers->EnumerateHeader(nullptr, "Retry-After", &retry_string) &&
        net::HttpUtil::ParseRetryAfterHeader(retry_string, base::Time::Now(),
                                             &retry_after);
  }

  DCHECK(success) << head->headers->raw_headers();
  std::move(callback_).Run(Response{success, response_code}, retry_after,
                           std::string());
}

void BoxCommitUploadSessionApiCallFlow::ProcessFailure(Response response) {
  std::move(callback_).Run(response, base::TimeDelta(), std::string());
}

}  // namespace enterprise_connectors

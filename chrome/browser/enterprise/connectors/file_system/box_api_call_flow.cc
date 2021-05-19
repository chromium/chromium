// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include <string>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_endpoints.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "rename_handler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#define LOG_API_FAIL(severity, flow, net_error, headers, body) \
  DLOG(severity) << "[BoxApiCallFlow] " << flow                \
                 << " failed; net error = " << net_error       \
                 << "; code = " << headers->response_code()    \
                 << ";\nheader: " << headers->raw_headers()    \
                 << ";\nbody: " << (body ? *body : "<null>");

#define LOG_PARSE_FAIL(severity, flow, result)                             \
  DLOG(severity) << "[BoxApiCallFlow] " << flow << " OnJsonParsed Error: " \
                 << (result.error ? result.error->data() : "<no error info>");

#define LOG_PARSE_FAIL_IF(log_condition, severity, flow, result) \
  if (log_condition) {                                           \
    LOG_PARSE_FAIL(severity, flow, result);                      \
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

// For possible extensions:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
std::string GetMimeType(base::FilePath file_path) {
  auto ext = file_path.FinalExtension();
  if (ext.front() == '.') {
    ext.erase(ext.begin());
  }
  DCHECK_NE(ext, FILE_PATH_LITERAL("crdownload"));

  std::string file_type;
  bool result = net::GetMimeTypeFromExtension(ext, &file_type);
  DCHECK(result || file_type.empty());
  return file_type;
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
  auto parts_list = parts.FindPath("parts")->GetList();
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
  if (!result.value) {
    return false;
  }

  const base::Value* entries = result.value->FindPath("entries");
  if (!entries || !entries->is_list()) {
    return false;
  }

  CHECK(list);
  *list = entries->GetList();
  return true;
}

GURL ExtractUploadedFileUrl(const Box::ParseResult& result) {
  base::Value::ConstListView list;
  std::string file_id;
  if (ExtractEntriesList(result, &list) && !list.empty()) {
    file_id = ExtractId(list.front());
  }
  LOG_PARSE_FAIL_IF(file_id.empty(), ERROR, "ExtractUploadedFileUrl", result);
  return file_id.empty() ? GURL() : Box::MakeUrlToShowFile(file_id);
}

void ProcessUploadSuccessResponse(const network::mojom::URLResponseHead* head,
                                  std::unique_ptr<std::string> body,
                                  base::OnceCallback<void(GURL)> callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(
                 [](decltype(callback) cb, Box::ParseResult result) {
                   auto url = ExtractUploadedFileUrl(result);
                   std::move(cb).Run(url);
                 },
                 std::move(callback)));
}

}  // namespace

namespace enterprise_connectors {

// File size limit according to https://developer.box.com/guides/uploads/:
// - Chucked upload APIs is only supported for file size >= 20 MB;
// - Whole file upload API is only supported for file size <= 50 MB.
const size_t BoxApiCallFlow::kChunkFileUploadMinSize =
    20 * 1024 * 1024;  // 20 MB
const size_t BoxApiCallFlow::kWholeFileUploadMaxSize =
    50 * 1024 * 1024;  // 50 MB

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
  DCHECK(file_id.size());
  return GURL("https://app.box.com/file/").Resolve(file_id);
}

// static
GURL BoxApiCallFlow::MakeUrlToShowFolder(const std::string& folder_id) {
  return folder_id.empty()
             ? GURL()
             : GURL("https://app.box.com/folder/").Resolve(folder_id);
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
      *body, base::BindOnce(&BoxFindUpstreamFolderApiCallFlow::OnJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxFindUpstreamFolderApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "FindUpstreamFolder", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, std::string());
}

void BoxFindUpstreamFolderApiCallFlow::OnJsonParsed(ParseResult result) {
  base::Value::ConstListView list;
  bool extract_entries = ExtractEntriesList(result, &list);
  if (extract_entries && !list.empty()) {
    std::string folder_id = ExtractId(list.front());
    std::move(callback_).Run(!folder_id.empty(), net::HTTP_OK, folder_id);
  } else {
    LOG_PARSE_FAIL_IF(!extract_entries, ERROR, "FindUpstreamFolder", result);
    std::move(callback_).Run(extract_entries, net::HTTP_OK, std::string());
  }
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
  return code == net::HTTP_CREATED;
}

void BoxCreateUpstreamFolderApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  auto response_code = head->headers->response_code();
  CHECK_EQ(response_code, net::HTTP_CREATED);

  data_decoder::DataDecoder::ParseJsonIsolated(
      *body, base::BindOnce(&BoxCreateUpstreamFolderApiCallFlow::OnJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxCreateUpstreamFolderApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "CreateUpstreamFolder", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, std::string());
}

void BoxCreateUpstreamFolderApiCallFlow::OnJsonParsed(ParseResult result) {
  std::string folder_id;
  if (result.value) {
    folder_id = ExtractId(*result.value);
  }
  LOG_PARSE_FAIL_IF(folder_id.empty(), ERROR, "CreateUpstreamFolder", result);
  std::move(callback_).Run(!folder_id.empty(), net::HTTP_CREATED, folder_id);
  return;
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
  std::move(callback_).Run(true, response_code);
}

void BoxPreflightCheckApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "PreflightCheck", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code);
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
    const base::FilePath& target_file_name,
    const base::FilePath& local_file_path)
    : folder_id_(folder_id),
      target_file_name_(target_file_name),
      local_file_path_(local_file_path),
      multipart_boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)) {}

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
                                       local_file_path_, target_file_name_);
  auto read_file_reply = base::BindOnce(
      &BoxWholeFileUploadApiCallFlow::OnFileRead, weak_factory_.GetWeakPtr(),
      url_loader_factory, access_token);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      std::move(read_file_task), std::move(read_file_reply));
}

// static
absl::optional<BoxWholeFileUploadApiCallFlow::FileRead>
BoxWholeFileUploadApiCallFlow::ReadFile(
    const base::FilePath& path,
    const base::FilePath& target_file_name) {
  FileRead file_read;
  file_read.mime = GetMimeType(target_file_name);
  if (file_read.mime.empty() ||  // Ensure that file extension was valid.
      !base::ReadFileToStringWithMaxSize(path, &file_read.content,
                                         kWholeFileUploadMaxSize)) {
    DLOG(ERROR) << "File " << path << " with target name " << target_file_name;
    return absl::nullopt;
  }
  DCHECK_LE(file_read.content.size(), kWholeFileUploadMaxSize);
  return absl::optional<FileRead>(std::move(file_read));
}

void BoxWholeFileUploadApiCallFlow::OnFileRead(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    absl::optional<FileRead> file_read) {
  if (!file_read) {
    DLOG(ERROR) << "[BoxApiCallFlow] WholeFileUpload read file failed";
    // TODO(https://crbug.com/1165972): error handling
    std::move(callback_).Run(false, 0, GURL());
    return;
  }
  DCHECK_LE(file_read->content.size(), kWholeFileUploadMaxSize);
  file_read_ = std::move(*file_read);

  // Continue to the original call flow after file has been read.
  OAuth2ApiCallFlow::Start(url_loader_factory, access_token);
}

GURL BoxWholeFileUploadApiCallFlow::CreateApiCallUrl() {
  return GURL(kFileSystemBoxEndpointWholeFileUpload);
}

std::string BoxWholeFileUploadApiCallFlow::CreateApiCallBody() {
  CHECK(!folder_id_.empty());
  CHECK(!target_file_name_.empty());
  CHECK(!file_read_.mime.empty()) << target_file_name_;
  CHECK(!multipart_boundary_.empty());

  base::Value attr(base::Value::Type::DICTIONARY);
  attr.SetStringKey("name", target_file_name_.MaybeAsASCII());
  attr.SetKey("parent", CreateSingleFieldDict("id", folder_id_));

  std::string attr_json;
  base::JSONWriter::Write(attr, &attr_json);

  std::string body;
  net::AddMultipartValueForUpload("attributes", attr_json, multipart_boundary_,
                                  "application/json", &body);

  net::AddMultipartValueForUploadWithFileName(
      "file", target_file_name_.MaybeAsASCII(), file_read_.content,
      multipart_boundary_, file_read_.mime, &body);
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
      head, std::move(body),
      base::BindOnce(std::move(callback_), true, net::HTTP_CREATED));
}

void BoxWholeFileUploadApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "WholeFileUpload", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, GURL());
}

void BoxWholeFileUploadApiCallFlow::SetFileReadForTesting(
    std::string content,
    std::string mime_type) {
  file_read_.content = std::move(content);
  file_read_.mime = std::move(mime_type);
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
      *body, base::BindOnce(&BoxCreateUploadSessionApiCallFlow::OnJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxCreateUploadSessionApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "CreateUploadSession", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, CreateEmptyDict(), 0);
}

void BoxCreateUploadSessionApiCallFlow::OnJsonParsed(ParseResult result) {
  bool valid_response = result.value.has_value();
  if (!valid_response) {
    LOG_PARSE_FAIL(ERROR, "CreateUploadSession", result);
    std::move(callback_).Run(false, net::HTTP_CREATED, CreateEmptyDict(), 0);
    return;
  }

  auto* endpoints = result.value->FindPath("session_endpoints");
  auto* part_size = result.value->FindPath("part_size");
  valid_response =
      endpoints && part_size && endpoints->FindPath("upload_part") &&
      endpoints->FindPath("commit") && endpoints->FindPath("abort");
  if (!valid_response) {
    LOG(ERROR) << "[BoxApiCallFlow] CreateUploadSession succeeded but "
                  "response returned is invalid: "
               << *result.value;
    std::move(callback_).Run(false, net::HTTP_CREATED, CreateEmptyDict(), 0);
  } else {
    std::move(callback_).Run(true, net::HTTP_CREATED, std::move(*endpoints),
                             part_size->GetInt());
  }
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
  CHECK(!body.empty());
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
      *body, base::BindOnce(&BoxPartFileUploadApiCallFlow::OnJsonParsed,
                            weak_factory_.GetWeakPtr()));
}

void BoxPartFileUploadApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "PartFileUpload", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, base::Value());
}

void BoxPartFileUploadApiCallFlow::OnJsonParsed(ParseResult result) {
  if (!result.value) {
    LOG_PARSE_FAIL(ERROR, "PartFileUpload", result);
    std::move(callback_).Run(false, net::HTTP_OK, base::Value());
    return;
  }

  base::Value* part = result.value->FindPath("part");
  if (!part) {
    DLOG(ERROR) << "[BoxApiCallFlow] No info for uploaded part";
    std::move(callback_).Run(false, net::HTTP_OK, base::Value());
  } else {
    std::move(callback_).Run(true, net::HTTP_OK, std::move(*part));
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
  std::move(callback_).Run(!body || body->empty(),  // Expecting an empty body.
                           head->headers->response_code());
}

void BoxAbortUploadSessionApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::move(callback_).Run(false, head->headers->response_code());
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
    auto created_cb = base::BindOnce(std::move(callback_), true, response_code,
                                     base::TimeDelta());
    return ProcessUploadSuccessResponse(head, std::move(body),
                                        std::move(created_cb));
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
  std::move(callback_).Run(success, response_code, retry_after, GURL());
}

void BoxCommitUploadSessionApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "CommitUploadSession", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, base::TimeDelta(), GURL());
}

}  // namespace enterprise_connectors

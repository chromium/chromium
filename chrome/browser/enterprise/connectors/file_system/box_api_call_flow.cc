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

#define LOG_API_FAIL(severity, class, net_error, headers, body)   \
  DLOG(severity) << "[BoxApiCallFlow] "                           \
                 << class << " failed; net error = " << net_error \
                 << "; code = " << headers->response_code()       \
                 << ";\nheader: " << headers->raw_headers()       \
                 << ";\nbody: " << (body ? *body : "<null>");

#define LOG_PARSE_FAIL(severity, class, result)                             \
  DLOG(severity) << "[BoxApiCallFlow] " << class << " OnJsonParsed Error: " \
                 << (result.error ? result.error->data() : "<no error info>");

namespace {

// Create folder at root.
static const char kParentFolderId[] = "0";

std::string ExtractFolderId(const base::Value& entry) {
  const base::Value* folder_id = entry.FindPath("id");
  if (!folder_id) {
    DLOG(ERROR) << "[BoxApiCallFlow] Can't find folder id! " << entry;
    return std::string();
  }

  std::string id;
  if (folder_id->type() == base::Value::Type::STRING) {
    id = folder_id->GetString();
  } else if (folder_id->type() == base::Value::Type::INTEGER) {
    id = base::NumberToString(folder_id->GetInt());
  } else {
    const char* type_name = folder_id->GetTypeName(folder_id->type());
    DLOG(ERROR) << "[BoxApiCallFlow] Invalid folder_id type: " << type_name;
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

void BoxFindUpstreamFolderApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    LOG_PARSE_FAIL(ERROR, "FindUpstreamFolder", result);
    std::move(callback_).Run(false, net::HTTP_OK, std::string());
    return;
  }

  const base::Value* entries = result.value->FindPath("entries");
  if (entries && entries->is_list()) {
    auto entries_list = entries->GetList();
    if (!entries_list.empty()) {
      std::string folder_id = ExtractFolderId(entries_list.front());
      std::move(callback_).Run(!folder_id.empty(), net::HTTP_OK, folder_id);
    } else {
      // Can't find folder, so return empty id but success status.
      std::move(callback_).Run(true, net::HTTP_OK, std::string());
    }
    return;
  }

  DLOG(ERROR) << "[BoxApiCallFlow] FindUpstreamFolder returned invalid entries";
  std::move(callback_).Run(false, net::HTTP_OK, std::string());
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

void BoxCreateUpstreamFolderApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  std::string folder_id;
  if (result.value) {
    folder_id = ExtractFolderId(*result.value);
  } else {
    LOG_PARSE_FAIL(ERROR, "CreateUpstreamFolder", result);
  }
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
      file_mime_type_(GetMimeType(target_file_name)),
      multipart_boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)) {}

BoxWholeFileUploadApiCallFlow::~BoxWholeFileUploadApiCallFlow() = default;

void BoxWholeFileUploadApiCallFlow::Start(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  // Ensure that file extension was valid and file type was obtained.
  if (file_mime_type_.empty()) {
    DLOG(ERROR) << "Couldn't obtain proper file type for " << target_file_name_;
    std::move(callback_).Run(false, 0);
  }

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

base::Optional<std::string> BoxWholeFileUploadApiCallFlow::ReadFile(
    const base::FilePath& path) {
  std::string content;
  return base::ReadFileToStringWithMaxSize(path, &content,
                                           kWholeFileUploadMaxSize)
             ? base::Optional<std::string>(std::move(content))
             : base::nullopt;
}

void BoxWholeFileUploadApiCallFlow::OnFileRead(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    base::Optional<std::string> file_read) {
  if (!file_read) {
    DLOG(ERROR) << "[BoxApiCallFlow] WholeFileUpload read file failed";
    // TODO(https://crbug.com/1165972): error handling
    std::move(callback_).Run(false, 0);
    return;
  }
  DCHECK_LE(file_read->size(), kWholeFileUploadMaxSize);
  file_content_ = std::move(*file_read);

  // Continue to the original call flow after file has been read.
  OAuth2ApiCallFlow::Start(url_loader_factory, access_token);
}

GURL BoxWholeFileUploadApiCallFlow::CreateApiCallUrl() {
  return GURL(kFileSystemBoxEndpointWholeFileUpload);
}

std::string BoxWholeFileUploadApiCallFlow::CreateApiCallBody() {
  CHECK(!folder_id_.empty());
  CHECK(!target_file_name_.empty());
  CHECK(!file_mime_type_.empty());
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
      "file", target_file_name_.MaybeAsASCII(), file_content_,
      multipart_boundary_, file_mime_type_, &body);
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
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(true, response_code);
}

void BoxWholeFileUploadApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "WholeFileUpload", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code);
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

void BoxCreateUploadSessionApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
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

void BoxPartFileUploadApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
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
  bool success = true;
  base::TimeDelta retry_after;
  if (response_code == net::HTTP_ACCEPTED) {
    std::string retry_string;
    success &=
        head->headers->EnumerateHeader(nullptr, "Retry-After", &retry_string);
    success &= net::HttpUtil::ParseRetryAfterHeader(
        retry_string, base::Time::Now(), &retry_after);
    DCHECK(success) << "Unable to find Retry-After header. Headers: "
                    << head->headers->raw_headers();
  }
  std::move(callback_).Run(success, response_code, retry_after);
}

void BoxCommitUploadSessionApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  LOG_API_FAIL(ERROR, "CommitUploadSession", net_error, head->headers, body);
  auto response_code = head->headers->response_code();
  std::move(callback_).Run(false, response_code, base::TimeDelta());
}

}  // namespace enterprise_connectors

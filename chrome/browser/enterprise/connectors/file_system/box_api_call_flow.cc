// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_endpoints.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

static const char kParentFolderId[] = "0";  // Create folder at root.

std::string ExtractFolderId(const base::Value& entry) {
  const base::Value* folder_id = entry.FindPath("id");
  if (!folder_id) {
    DLOG(ERROR) << "[BoxApiCallFlow] Can't find folder id!";
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

}  // namespace

namespace enterprise_connectors {

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
        chrome_policy {}
      })");
  // TODO(1157959): Add the policy that will turn on/off the connector here?
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
  auto response_code = head->headers->response_code();
  DLOG(ERROR)
      << "[BoxApiCallFlow] FindUpstreamFolder API call failed; net_error = "
      << net_error << "; response_code = " << response_code;
  std::move(callback_).Run(false, response_code, std::string());
}

void BoxFindUpstreamFolderApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    DLOG(ERROR) << "[BoxApiCallFlow] FindUpstreamFolder OnJsonParsed Error: "
                << result.error->data();
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
  return;
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

  base::Value parent_val(base::Value::Type::DICTIONARY);
  parent_val.SetStringKey("id", kParentFolderId);
  val.SetKey("parent", std::move(parent_val));

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
  auto response_code = head->headers->response_code();
  DLOG(ERROR)
      << "[BoxApiCallFlow] CreateUpstreamFolder API call failed; net error = "
      << net_error << "; response code = " << response_code;
  std::move(callback_).Run(false, response_code, std::string());
}

void BoxCreateUpstreamFolderApiCallFlow::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  std::string folder_id;
  if (result.value) {
    folder_id = ExtractFolderId(*result.value);
  } else {
    DLOG(ERROR) << "[BoxApiCallFlow] CreateUpstreamFolder OnJsonParsed Error: "
                << result.error->data();
  }
  // TODO(1157641): store folder_id in profile pref to handle indexing latency.
  std::move(callback_).Run(!folder_id.empty(), net::HTTP_OK, folder_id);
  return;
}

}  // namespace enterprise_connectors

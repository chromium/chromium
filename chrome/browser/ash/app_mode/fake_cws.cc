// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/fake_cws.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/initialize_extensions_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "crypto/sha2.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace ash {

namespace {

// Kiosk app crx file download path under web store site.
constexpr std::string_view kCrxDownloadPath =
    "/chromeos/app_mode/webstore/downloads/";

constexpr std::string_view kDetailsURLPrefix =
    "/chromeos/app_mode/webstore/inlineinstall/detail/";

constexpr std::string_view kItemSnippetsURLPrefix =
    "/chromeos/app_mode/webstore/itemsnippet/";

constexpr std::string_view kAppNoUpdateTemplate =
    "<app appid=\"$AppId\" status=\"ok\">"
    "<updatecheck status=\"noupdate\"/>"
    "</app>";

constexpr std::string_view kAppHasUpdateTemplate =
    "<app appid=\"$AppId\" status=\"ok\">"
    "<updatecheck codebase=\"$CrxDownloadUrl\" fp=\"1.$FP\" "
    "hash=\"\" hash_sha256=\"$FP\" size=\"$Size\" status=\"ok\" "
    "version=\"$Version\"/>"
    "</app>";

constexpr std::string_view kPrivateStoreAppHasUpdateTemplate =
    "<app appid=\"$AppId\">"
    "<updatecheck codebase=\"$CrxDownloadUrl\" version=\"$Version\"/>"
    "</app>";

constexpr std::string_view kUpdateContentTemplate =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<gupdate xmlns=\"http://www.google.com/update2/response\" "
    "protocol=\"2.0\" server=\"prod\">"
    "<daystart elapsed_days=\"2569\" elapsed_seconds=\"36478\"/>"
    "$APPS"
    "</gupdate>";

constexpr std::string_view kAppNoUpdateTemplateJSON =
    "{\"appid\": \"$AppId\","
    " \"status\": \"ok\","
    " \"updatecheck\": { \"status\": \"noupdate\" }"
    "}";

constexpr std::string_view kAppHasUpdateTemplateJSON =
    "{"
    "  \"appid\": \"$AppId\","
    "  \"status\": \"ok\","
    "  \"updatecheck\": {"
    "    \"status\": \"ok\","
    "    \"manifest\": {"
    "      \"version\": \"$Version\","
    "      \"packages\": {"
    "        \"package\": ["
    "          {"
    "            \"fp\": \"1.$FP\","
    "            \"size\": \"$Size\","
    "            \"hash_sha256\": \"$FP\","
    "            \"name\": \"\""
    "          }"
    "        ]"
    "      }"
    "    },"
    "    \"urls\": { \"url\": [ { \"codebase\": \"$CrxDownloadUrl\"} ] }"
    "  }"
    "}";

constexpr std::string_view kUpdateContentTemplateJSON =
    ")]}'\n"
    "{"
    "  \"response\": {"
    "    \"protocol\": \"3.1\","
    "    \"daystart\": {"
    "      \"elapsed_days\": 2569,"
    "      \"elapsed_seconds\": 36478"
    "    },"
    "    \"app\": ["
    "      $APPS"
    "    ]"
    "  }"
    "}";

constexpr std::string_view kAppIdHeader = "X-Goog-Update-AppId";

bool GetAppIdsFromHeader(const HttpRequest::HeaderMap& headers,
                         std::vector<std::string>* ids) {
  if (headers.count(kAppIdHeader) == 0) {
    return false;
  }
  base::StringTokenizer t(headers.at(std::string(kAppIdHeader)), ",");
  while (t.GetNext()) {
    ids->push_back(t.token());
  }
  return !ids->empty();
}

bool GetAppIdsFromUpdateUrl(const GURL& update_url,
                            std::vector<std::string>* ids) {
  for (net::QueryIterator it(update_url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() != "x") {
      continue;
    }
    std::string id;
    net::GetValueForKeyInQuery(GURL("http://dummy?" + it.GetUnescapedValue()),
                               "id", &id);
    ids->push_back(id);
  }
  return !ids->empty();
}

// The detail request has an URL in form of
// https://<domain>/chromeos/app_mode/webstore/inlineinstall/detail/<id>.
// Returns std::nullopt if the `request_path` doesn't look like request for
// extension details.
std::optional<std::string> GetAppIdFromDetailRequest(
    const std::string& request_path) {
  size_t prefix_length = kDetailsURLPrefix.size();
  if (request_path.substr(0, prefix_length) != kDetailsURLPrefix) {
    return std::nullopt;
  }
  return request_path.substr(prefix_length);
}

// Returns the app ID from `request_path` if the request's URL looks like one
// used to fetch an item snippet.
std::optional<std::string> GetAppIdFromItemSnippetsRequest(
    const std::string& request_path) {
  size_t prefix_length = kItemSnippetsURLPrefix.size();
  if (request_path.substr(0, prefix_length) != kItemSnippetsURLPrefix) {
    return std::nullopt;
  }
  return request_path.substr(prefix_length);
}

// FakeCWS uses ScopedIgnoreContentVerifierForTest to disable extension
// content verification. This helper could be instantiated only once. Usually
// that not an issue, since FakeCWS is also instantiated only once, in a base
// test class. Some tests use a secondary FakeCWS instance. This flag will be
// set by first created FakeCWS (which we'll call "primary"), and only primary
// FakeCWS will hold the ScopedIgnoreContentVerifierForTest instance.
bool g_is_fakecws_active = false;

std::string ApplyHasNoUpdateTemplate(std::string app_id,
                                     bool use_json,
                                     bool use_private_store) {
  std::string update_check_content(use_json ? kAppNoUpdateTemplateJSON
                                            : kAppNoUpdateTemplate);
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0, "$AppId",
                                     app_id);
  return update_check_content;
}

std::string ApplyHasUpdateTemplate(std::string app_id,
                                   GURL download_url,
                                   std::string sha256_hex,
                                   int size,
                                   std::string version,
                                   bool use_json,
                                   bool use_private_store) {
  std::string update_check_content(use_json ? kAppHasUpdateTemplateJSON
                                   : use_private_store
                                       ? kPrivateStoreAppHasUpdateTemplate
                                       : kAppHasUpdateTemplate);
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0, "$AppId",
                                     app_id);
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0,
                                     "$CrxDownloadUrl", download_url.spec());
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0, "$FP",
                                     sha256_hex);
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0, "$Size",
                                     base::NumberToString(size));
  base::ReplaceSubstringsAfterOffset(&update_check_content, 0, "$Version",
                                     version);
  return update_check_content;
}

// Serve serialized FetchItemSnippet protos stored under gen/chrome/test/data.
// The serialized protos are generated from the textproto files in
// //chrome/test/data/chromeos/app_mode/webstore/itemsnippet.
void ServeFilesFromGeneratedDirectory(
    net::EmbeddedTestServer& embedded_test_server) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &test_data_dir));

  test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("chrome/test/data"));
  embedded_test_server.ServeFilesFromDirectory(test_data_dir);
}

}  // namespace

FakeCWS::FakeCWS() : update_check_count_(0) {
  if (!g_is_fakecws_active) {
    g_is_fakecws_active = true;
    scoped_ignore_content_verifier_ =
        std::make_unique<extensions::ScopedIgnoreContentVerifierForTest>();
  }
}

FakeCWS::~FakeCWS() {
  // If the secondary FakeCWS was desructed after primary one, secondary will
  // work without scoped_ignore_content_verifier_. We want to catch such a
  // situation, so we check that primary FakeCWS is not destroyed yet.
  DCHECK(g_is_fakecws_active);

  if (scoped_ignore_content_verifier_) {
    g_is_fakecws_active = false;
  }
}

void FakeCWS::Init(net::EmbeddedTestServer* embedded_test_server) {
  use_private_store_templates_ = false;
  update_check_end_point_ = "/update_check.xml";

  SetupWebStoreURL(embedded_test_server->base_url());
  OverrideGalleryCommandlineSwitches();
  embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&FakeCWS::HandleRequest, base::Unretained(this)));

  ServeFilesFromGeneratedDirectory(CHECK_DEREF(embedded_test_server));
}

void FakeCWS::InitAsPrivateStore(net::EmbeddedTestServer* embedded_test_server,
                                 std::string_view update_check_end_point) {
  use_private_store_templates_ = true;
  update_check_end_point_ = update_check_end_point;

  SetupWebStoreURL(embedded_test_server->base_url());
  OverrideGalleryCommandlineSwitches();

  embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&FakeCWS::HandleRequest, base::Unretained(this)));

  ServeFilesFromGeneratedDirectory(CHECK_DEREF(embedded_test_server));
}

void FakeCWS::SetUpdateCrx(std::string_view app_id,
                           std::string_view crx_file,
                           std::string_view version) {
  GURL crx_download_url =
      web_store_url_.Resolve(base::StrCat({kCrxDownloadPath, crx_file}));

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath crx_file_path =
      test_data_dir.AppendASCII("chromeos/app_mode/webstore/downloads")
          .AppendASCII(crx_file);
  std::string crx_content;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::ReadFileToString(crx_file_path, &crx_content));
  }

  const std::string sha256 = crypto::SHA256HashString(crx_content);
  const std::string sha256_hex = base::HexEncode(sha256);

  std::string app_id_str(app_id);
  id_to_update_check_content_map_[app_id_str] =
      base::BindRepeating(&ApplyHasUpdateTemplate, app_id_str, crx_download_url,
                          sha256_hex, crx_content.size(), std::string(version));
}

void FakeCWS::SetNoUpdate(std::string_view app_id) {
  std::string app_id_str(app_id);
  id_to_update_check_content_map_[app_id_str] =
      base::BindRepeating(&ApplyHasNoUpdateTemplate, app_id_str);
}

void FakeCWS::SetAppDetails(std::string_view app_id,
                            std::string localized_name,
                            std::string icon_url,
                            std::string manifest_json) {
  id_to_details_map_[std::string(app_id)] =
      AppDetails{.localized_name = std::move(localized_name),
                 .icon_url = std::move(icon_url),
                 .manifest_json = std::move(manifest_json)};
}

int FakeCWS::GetUpdateCheckCountAndReset() {
  int current_count = update_check_count_;
  update_check_count_ = 0;
  return current_count;
}

std::optional<std::string> FakeCWS::CreateItemSnippetStringForApp(
    const std::string& app_id) {
  auto it = id_to_details_map_.find(app_id);
  if (it == id_to_details_map_.end()) {
    return std::nullopt;
  }

  const AppDetails& app_details = it->second;

  extensions::FetchItemSnippetResponse item_snippet;
  item_snippet.set_item_id(app_id);
  item_snippet.set_manifest(app_details.manifest_json);
  item_snippet.set_title(app_details.localized_name);
  item_snippet.set_logo_uri(app_details.icon_url);

  // Default values.
  item_snippet.set_summary("");
  item_snippet.set_user_count_string("0");
  item_snippet.set_rating_count_string("0");
  item_snippet.set_rating_count(0);
  item_snippet.set_average_rating(0.0);

  std::string item_snippet_string;
  if (!item_snippet.SerializeToString(&item_snippet_string)) {
    return std::nullopt;
  }

  return item_snippet_string;
}

void FakeCWS::SetupWebStoreURL(const GURL& test_server_url) {
  web_store_url_ = test_server_url;

  // Replace part of the item snippets URL with the `web_store_url_` with the
  // embedded test server's port so requests can be handled in `HandleRequest`.
  item_snippets_url_ = web_store_url_.Resolve(kItemSnippetsURLPrefix);
  item_snippets_url_override_ =
      extension_urls::SetItemSnippetURLForTesting(&item_snippets_url_);
}

void FakeCWS::OverrideGalleryCommandlineSwitches() {
  DCHECK(web_store_url_.is_valid());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  command_line->AppendSwitchASCII(
      ::switches::kAppsGalleryURL,
      web_store_url_.Resolve("/chromeos/app_mode/webstore").spec());

  GURL downloads_url =
      web_store_url_.Resolve(base::StrCat({kCrxDownloadPath, "%s.crx"}));
  command_line->AppendSwitchASCII(::switches::kAppsGalleryDownloadURL,
                                  downloads_url.spec());

  GURL update_url = web_store_url_.Resolve(update_check_end_point_);
  command_line->AppendSwitchASCII(::switches::kAppsGalleryUpdateURL,
                                  update_url.spec());

  EnsureExtensionsClientInitialized();
  extensions::ExtensionsClient::Get()->InitializeWebStoreUrls(command_line);
}

bool FakeCWS::GetUpdateCheckContent(const std::vector<std::string>& ids,
                                    std::string* update_check_content,
                                    bool use_json) {
  std::string apps_content;
  bool need_comma = false;
  for (const std::string& id : ids) {
    std::string app_update_content;
    auto it = id_to_update_check_content_map_.find(id);
    if (it == id_to_update_check_content_map_.end()) {
      return false;
    }
    if (need_comma) {
      apps_content.append(",");
    }
    apps_content.append(it->second.Run(use_json, use_private_store_templates_));
    need_comma = use_json;
  }
  if (apps_content.empty()) {
    return false;
  }

  *update_check_content =
      use_json ? kUpdateContentTemplateJSON : kUpdateContentTemplate;
  base::ReplaceSubstringsAfterOffset(update_check_content, 0, "$APPS",
                                     apps_content);
  return true;
}

std::unique_ptr<HttpResponse> FakeCWS::HandleRequest(
    const HttpRequest& request) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();
  if (request_path.find(update_check_end_point_) != std::string::npos &&
      !id_to_update_check_content_map_.empty()) {
    std::vector<std::string> ids;
    if (GetAppIdsFromHeader(request.headers, &ids) ||
        GetAppIdsFromUpdateUrl(request_url, &ids)) {
      bool use_json =
          request.content.size() > 0 && request.content.at(0) == '{';
      std::string update_check_content;
      if (GetUpdateCheckContent(ids, &update_check_content, use_json)) {
        ++update_check_count_;
        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        if (!use_json) {
          http_response->set_content_type("text/xml");
        }
        http_response->set_content(update_check_content);
        return std::move(http_response);
      }
    }
  }

  std::optional<std::string> details_id =
      GetAppIdFromDetailRequest(request_path);
  if (details_id) {
    auto it = id_to_details_map_.find(*details_id);
    if (it != id_to_details_map_.end()) {
      std::string details =
          base::WriteJson(base::Value::Dict()
                              .Set("id", *details_id)
                              .Set("icon_url", it->second.icon_url)
                              .Set("localized_name", it->second.localized_name)
                              .Set("manifest", it->second.manifest_json))
              .value();
      auto http_response = std::make_unique<BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("application/json");
      http_response->set_content(details);
      return std::move(http_response);
    }
  }

  std::optional<std::string> app_id =
      GetAppIdFromItemSnippetsRequest(request_path);
  if (app_id) {
    std::optional<std::string> item_snippet_response =
        CreateItemSnippetStringForApp(app_id.value());
    if (item_snippet_response) {
      auto http_response = std::make_unique<BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("application/x-protobuf");
      http_response->set_content(item_snippet_response.value());
      return std::move(http_response);
    }
  }

  return nullptr;
}

}  // namespace ash

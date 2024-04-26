// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_data.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#endif

namespace {

const char kNewTabOgbApiPath[] = "/async/newtab_ogb";

const char kResponsePreamble[] = ")]}'";

// This namespace contains helpers to extract SafeHtml-wrapped strings (see
// https://github.com/google/safe-html-types) from the response json. If there
// is ever a C++ version of the SafeHtml types, we should consider using that
// instead of these custom functions.
namespace safe_html {

bool GetImpl(const base::Value::Dict& dict,
             const std::string& name,
             const std::string& wrapped_field_name,
             std::string* out) {
  const base::Value::Dict* value = dict.FindDict(name);
  if (!value) {
    out->clear();
    return false;
  }

  const std::string* maybe_field_val = value->FindString(wrapped_field_name);
  if (!maybe_field_val) {
    out->clear();
    return false;
  }
  *out = *maybe_field_val;

  return true;
}

bool GetHtml(const base::Value::Dict& dict,
             const std::string& name,
             std::string* out) {
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_html_wrapped_value", out);
}

bool GetScript(const base::Value::Dict& dict,
               const std::string& name,
               std::string* out) {
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_script_wrapped_value",
                 out);
}

bool GetStyleSheet(const base::Value::Dict& dict,
                   const std::string& name,
                   std::string* out) {
  return GetImpl(dict, name,
                 "private_do_not_access_or_else_safe_style_sheet_wrapped_value",
                 out);
}

}  // namespace safe_html

std::optional<OneGoogleBarData> JsonToOGBData(const base::Value& value) {
  if (!value.is_dict()) {
    DVLOG(1) << "Parse error: top-level dictionary not found";
    return std::nullopt;
  }
  const base::Value::Dict& dict = value.GetDict();

  const base::Value::Dict* update = dict.FindDict("update");
  if (!update) {
    DVLOG(1) << "Parse error: no update";
    return std::nullopt;
  }

  const std::string* maybe_language = update->FindString("language_code");
  std::string language_code;
  if (maybe_language) {
    language_code = *maybe_language;
  }

  const base::Value::Dict* one_google_bar = update->FindDict("ogb");
  if (!one_google_bar) {
    DVLOG(1) << "Parse error: no ogb";
    return std::nullopt;
  }

  OneGoogleBarData result;
  result.language_code = language_code;

  if (!safe_html::GetHtml(*one_google_bar, "html", &result.bar_html)) {
    DVLOG(1) << "Parse error: no html";
    return std::nullopt;
  }

  const base::Value::Dict* page_hooks = one_google_bar->FindDict("page_hooks");
  if (!page_hooks) {
    DVLOG(1) << "Parse error: no page_hooks";
    return std::nullopt;
  }

  safe_html::GetScript(*page_hooks, "in_head_script", &result.in_head_script);
  safe_html::GetStyleSheet(*page_hooks, "in_head_style", &result.in_head_style);
  safe_html::GetScript(*page_hooks, "after_bar_script",
                       &result.after_bar_script);
  safe_html::GetHtml(*page_hooks, "end_of_body_html", &result.end_of_body_html);
  safe_html::GetScript(*page_hooks, "end_of_body_script",
                       &result.end_of_body_script);

  return result;
}

}  // namespace

class OneGoogleBarLoaderImpl::AuthenticatedURLLoader {
 public:
  using LoadDoneCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* simple_loader,
                              std::unique_ptr<std::string> response_body)>;

  AuthenticatedURLLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL api_url,
      bool account_consistency_mirror_required,
      LoadDoneCallback callback);
  ~AuthenticatedURLLoader() = default;

  void Start();

 private:
  void SetRequestHeaders(network::ResourceRequest* request) const;

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL api_url_;
#if BUILDFLAG(IS_CHROMEOS)
  const bool account_consistency_mirror_required_;
#endif

  LoadDoneCallback callback_;

  // The underlying SimpleURLLoader which does the actual load.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
};

OneGoogleBarLoaderImpl::AuthenticatedURLLoader::AuthenticatedURLLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL api_url,
    bool account_consistency_mirror_required,
    LoadDoneCallback callback)
    : url_loader_factory_(url_loader_factory),
      api_url_(std::move(api_url)),
#if BUILDFLAG(IS_CHROMEOS)
      account_consistency_mirror_required_(account_consistency_mirror_required),
#endif
      callback_(std::move(callback)) {
}

void OneGoogleBarLoaderImpl::AuthenticatedURLLoader::SetRequestHeaders(
    network::ResourceRequest* request) const {
  variations::AppendVariationsHeaderUnknownSignedIn(
      api_url_, variations::InIncognito::kNo, request);
#if BUILDFLAG(IS_CHROMEOS)
  signin::ChromeConnectedHeaderHelper chrome_connected_header_helper(
      account_consistency_mirror_required_
          ? signin::AccountConsistencyMethod::kMirror
          : signin::AccountConsistencyMethod::kDisabled);
  int profile_mode = signin::PROFILE_MODE_DEFAULT;
  if (account_consistency_mirror_required_) {
    // For the child account case (where currently
    // |account_consistency_mirror_required_| is true on Chrome OS), we always
    // want to disable adding an account and going to incognito.
    profile_mode = signin::PROFILE_MODE_INCOGNITO_DISABLED |
                   signin::PROFILE_MODE_ADD_ACCOUNT_DISABLED;
  }

  // TODO(crbug.com/40151268): Check whether the child account status should
  // also be sent in the Mirror request header when loading the local version of
  // OneGoogleBar.
  std::string chrome_connected_header_value =
      chrome_connected_header_helper.BuildRequestHeader(
          /*is_header_request=*/true, api_url_,
          // Gaia ID is only needed for (drive|docs).google.com.
          /*gaia_id=*/std::string(),
          /*is_child_account=*/signin::Tribool::kUnknown, profile_mode,
          signin::kChromeMirrorHeaderSource,
          /*force_account_consistency=*/false);
  if (!chrome_connected_header_value.empty()) {
    request->headers.SetHeader(signin::kChromeConnectedHeader,
                               chrome_connected_header_value);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OneGoogleBarLoaderImpl::AuthenticatedURLLoader::Start() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("one_google_bar_service", R"(
        semantics {
          sender: "One Google Bar Service"
          description: "Downloads the 'One Google' bar."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider."
          data: "Credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = api_url_;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  SetRequestHeaders(resource_request.get());
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
  // Adds cookies even if 3P cookies are blocked. See b/297160590.
  resource_request->site_for_cookies = net::SiteForCookies::FromUrl(api_url_);

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &OneGoogleBarLoaderImpl::AuthenticatedURLLoader::OnURLLoaderComplete,
          base::Unretained(this)),
      1024 * 1024);
}

void OneGoogleBarLoaderImpl::AuthenticatedURLLoader::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  std::move(callback_).Run(simple_loader_.get(), std::move(response_body));
}

OneGoogleBarLoaderImpl::OneGoogleBarLoaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& application_locale,
    bool account_consistency_mirror_required)
    : url_loader_factory_(url_loader_factory),
      application_locale_(application_locale),
      account_consistency_mirror_required_(
          account_consistency_mirror_required) {}

OneGoogleBarLoaderImpl::~OneGoogleBarLoaderImpl() = default;

void OneGoogleBarLoaderImpl::Load(OneGoogleCallback callback) {
  callbacks_.push_back(std::move(callback));

  // Note: If there is an ongoing request, abandon it. It's possible that
  // something has changed in the meantime (e.g. signin state) that would make
  // the result obsolete.
  pending_request_ = std::make_unique<AuthenticatedURLLoader>(
      url_loader_factory_, GetApiUrl(), account_consistency_mirror_required_,
      base::BindOnce(&OneGoogleBarLoaderImpl::LoadDone,
                     base::Unretained(this)));
  pending_request_->Start();
}

GURL OneGoogleBarLoaderImpl::GetLoadURLForTesting() const {
  return GetApiUrl();
}

bool OneGoogleBarLoaderImpl::SetAdditionalQueryParams(
    const std::string& value) {
  if (additional_query_params_ == value) {
    return false;
  }
  additional_query_params_ = value;
  return true;
}

GURL OneGoogleBarLoaderImpl::GetApiUrl() const {
  GURL api_url;
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }

  api_url = google_base_url.Resolve(kNewTabOgbApiPath);

  // Add the "hl=" parameter.
  if (additional_query_params_.find("&hl=") == std::string::npos) {
    api_url = net::AppendQueryParameter(api_url, "hl", application_locale_);
  }

  // Add the "async=" parameter. We can't use net::AppendQueryParameter for
  // this because we need the ":" to remain unescaped.
  GURL::Replacements replacements;
  std::string query = api_url.query();
  query += additional_query_params_;
  if (additional_query_params_.find("&async=") == std::string::npos) {
    query += "&async=fixed:0";
  }
  if (query.at(0) == '&') {
    query = query.substr(1);
  }
  replacements.SetQueryStr(query);
  return api_url.ReplaceComponents(replacements);
}

void OneGoogleBarLoaderImpl::LoadDone(
    const network::SimpleURLLoader* simple_loader,
    std::unique_ptr<std::string> response_body) {
  // The loader will be deleted when the request is handled.
  std::unique_ptr<AuthenticatedURLLoader> deleter(std::move(pending_request_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << simple_loader->NetError();
    Respond(Status::TRANSIENT_ERROR, std::nullopt);
    return;
  }

  std::string response;
  response.swap(*response_body);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response, base::BindOnce(&OneGoogleBarLoaderImpl::JsonParsed,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OneGoogleBarLoaderImpl::JsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    DVLOG(1) << "Parsing JSON failed: " << result.error();
    Respond(Status::FATAL_ERROR, std::nullopt);
    return;
  }

  std::optional<OneGoogleBarData> data = JsonToOGBData(*result);
  Respond(data.has_value() ? Status::OK : Status::FATAL_ERROR, data);
}

void OneGoogleBarLoaderImpl::Respond(
    Status status,
    const std::optional<OneGoogleBarData>& data) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(status, data);
  }
  callbacks_.clear();
}

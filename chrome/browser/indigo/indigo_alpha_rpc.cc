// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_alpha_rpc.h"

#include <optional>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace indigo {

namespace {
constexpr int kClientError = -1;

std::optional<base::Value> JspbGet(const base::Value& value, unsigned index) {
  if (!value.is_list()) {
    return std::nullopt;
  }

  const base::ListValue& list = value.GetList();
  if (index > 0 && index - 1 < list.size()) {
    const base::Value& indexed_value = list[index - 1];
    if (!indexed_value.is_dict()) {
      return indexed_value.Clone();
    }
  }

  if (!list.empty()) {
    const base::Value& last = list.back();
    if (last.is_dict()) {
      const base::Value* found_value =
          last.GetDict().Find(base::NumberToString(index));
      if (found_value) {
        return found_value->Clone();
      }
    }
  }

  return std::nullopt;
}

base::expected<base::Value, std::string> GetJspbValue(std::string_view body) {
  if (size_t pos = body.find_first_of("[{"); pos != std::string_view::npos) {
    body.remove_prefix(pos);
  }
  base::JSONReader::Result parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(body, base::JSON_PARSE_RFC);
  if (!parsed_json.has_value()) {
    return base::unexpected(base::StrCat(
        {"Invalid JSON in response: ", parsed_json.error().ToString()}));
  }
  if (!parsed_json->is_dict()) {
    return base::unexpected("Response is not a dictionary");
  }
  std::optional<base::Value> jspb_value = parsed_json->GetDict().Extract("");
  if (!jspb_value) {
    return base::unexpected("JSPB body not found in response");
  }
  return base::ok(*std::move(jspb_value));
}
}  // namespace

base::expected<GURL, AlphaGenerateError> ParseAlphaGenerateResponse(
    std::string_view body) {
  // The response is a JSPB; we extract particular values based on their field
  // numbers. The underlying proto definition is internal, and this is only used
  // temporarily, so please excuse the use of magic numbers.
  auto jspb_value_or_error = GetJspbValue(body);
  if (!jspb_value_or_error.has_value()) {
    return base::unexpected(AlphaGenerateError{
        kClientError, std::move(jspb_value_or_error.error())});
  }
  const base::Value& jspb_value = *jspb_value_or_error;

  if (std::optional<base::Value> success = JspbGet(jspb_value, 1);
      success && !success->is_none()) {
    if (std::optional<base::Value> list_val = JspbGet(*success, 2);
        list_val && list_val->is_list() && !list_val->GetList().empty()) {
      if (std::optional<base::Value> url_val =
              JspbGet(list_val->GetList()[0], 1);
          url_val && url_val->is_string()) {
        return GURL(url_val->GetString());
      }
    }
    return base::unexpected(
        AlphaGenerateError{kClientError, "Malformed success response"});
  }

  if (std::optional<base::Value> failure = JspbGet(jspb_value, 3);
      failure && !failure->is_none()) {
    std::optional<base::Value> error_type = JspbGet(*failure, 1);
    if (!error_type || !error_type->is_int()) {
      return base::unexpected(
          AlphaGenerateError{kClientError, "Malformed failure response"});
    }

    std::string message = "No message";
    if (std::optional<base::Value> error_message = JspbGet(*failure, 2u);
        error_message && error_message->is_string()) {
      message = error_message->GetString();
    }

    return base::unexpected(AlphaGenerateError{error_type->GetInt(), message});
  }

  return base::unexpected(
      AlphaGenerateError{kClientError, "Malformed response"});
}

base::expected<void, std::string> ParseAlphaStatusResponse(
    std::string_view body) {
  auto jspb_value_or_error = GetJspbValue(body);
  if (!jspb_value_or_error.has_value()) {
    return base::unexpected(std::move(jspb_value_or_error.error()));
  }
  const base::Value& jspb_value = *jspb_value_or_error;

  std::optional<base::Value> status_value = JspbGet(jspb_value, 3);

  if (!status_value) {
    return base::unexpected("Status field not found in response.");
  }

  if (!status_value->is_int()) {
    return base::unexpected("Unexpected type for status field");
  }

  int status_int = status_value->GetInt();
  if (status_int != 3) {
    return base::unexpected(base::StrCat({"Unexpected value for status field: ",
                                          base::NumberToString(status_int)}));
  }

  return base::ok();
}

void ExecuteAlphaGenerateRpc(
    network::SharedURLLoaderFactory* loader_factory,
    base::OnceCallback<void(base::expected<GURL, AlphaGenerateError>)>
        callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(features::kIndigoAlphaGenerateUrl.Get());
  resource_request->method = "GET";
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;

  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("indigo_alpha_generate", R"(
        semantics {
          sender: "Indigo"
          description:
            "Generate new content for the Indigo feature. This is for internal "
            "experimentation only and must be removed or updated before launch."
            " b/487320384"
          trigger: "User activates the Indigo page action on an eligible page."
          user_data {
            type: WEB_CONTENT
            type: ACCESS_TOKEN
          }
          data: "An image from the web page."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/indigo/OWNERS"
            }
          }
          last_reviewed: "2026-02-20"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "n/a for now"
          policy_exception_justification:
            "Currently for internal prototyping only; policy controls will be "
            "added before any general use."
        })");

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_loader->DownloadToString(
      loader_factory,
      base::BindOnce(
          [](base::OnceCallback<void(base::expected<GURL, AlphaGenerateError>)>
                 callback,
             std::unique_ptr<network::SimpleURLLoader>,
             std::optional<std::string> response_body) {
            if (!response_body) {
              std::move(callback).Run(base::unexpected(
                  AlphaGenerateError{kClientError, "Net error"}));
              return;
            }
            std::move(callback).Run(ParseAlphaGenerateResponse(*response_body));
          },
          std::move(callback), std::move(simple_loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ExecuteAlphaStatusRpc(
    network::SharedURLLoaderFactory* loader_factory,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(features::kIndigoAlphaStatusUrl.Get());
  resource_request->method = "GET";
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;

  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("indigo_alpha_status", R"(
        semantics {
          sender: "Indigo"
          description:
            "Checks the status of content generation for the Indigo feature. "
            "This is for internal experimentation only and must be removed or "
            "updated before launch. b/487320384"
          trigger: "User activates the Indigo page action on an eligible page."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "User identity only."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/indigo/OWNERS"
            }
          }
          last_reviewed: "2026-02-20"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "n/a for now"
          policy_exception_justification:
            "Currently for internal prototyping only; policy controls will be "
            "added before any general use."
        })");

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_loader->DownloadToString(
      loader_factory,
      base::BindOnce(
          [](base::OnceCallback<void(base::expected<void, std::string>)>
                 callback,
             std::unique_ptr<network::SimpleURLLoader> loader,
             std::optional<std::string> response_body) {
            if (!response_body) {
              LOG(ERROR) << "Indigo Alpha Status Net error"
                         << loader->CompletionStatus()->error_code;
              std::move(callback).Run(base::unexpected("Net error"));
              return;
            }
            std::move(callback).Run(ParseAlphaStatusResponse(*response_body));
          },
          std::move(callback), std::move(simple_loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

}  // namespace indigo

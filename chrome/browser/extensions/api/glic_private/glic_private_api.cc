// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

// Convert invocation source to string to match the endpoint format.
std::string InvocationSourceToString(
    api::glic_private::InvocationSource source) {
  switch (source) {
    case api::glic_private::InvocationSource::kUniversalCart:
      return "INVOCATION_SOURCE_UNIVERSAL_CART";
    case api::glic_private::InvocationSource::kUnknown:
      return "INVOCATION_SOURCE_UNKNOWN";
    case api::glic_private::InvocationSource::kNone:
      return "INVOCATION_SOURCE_UNKNOWN";
  }
}

constexpr char kPromptId[] = "promptId";
constexpr char kInvocationSource[] = "invocationSource";
constexpr char kPrompt[] = "prompt";

using PromptCallback =
    base::OnceCallback<void(extensions::api::glic_private::ErrorCode,
                            std::optional<std::string>)>;

// LINT.IfChange(GlicPrivateApiStatusCodeHistogramValue)
enum class GlicPrivateApiStatusCodeHistogramValue {
  kSuccess = 0,
  kLocalInvalidInvocationSource = 1,
  kLocalMissingPromptId = 2,
  kServerMissingPrompt = 3,
  kHttpError = 4,
  kParseError = 5,
  kLocalNoActiveTab = 6,
  kLocalGlicNotEnabled = 7,
  kLocalGlicNotReady = 8,
  kLocalGlicActuationNotAllowed = 9,
  kLocalGlicNotEnabledAndConsented = 10,
  kMaxValue = kLocalGlicNotEnabledAndConsented,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicPrivateApiStatusCodeHistogramValue)

GlicPrivateApiStatusCodeHistogramValue ConvertStatusCodeToHistogramValue(
    extensions::api::glic_private::ErrorCode status) {
  switch (status) {
    case extensions::api::glic_private::ErrorCode::kNone:
      return GlicPrivateApiStatusCodeHistogramValue::kSuccess;
    case extensions::api::glic_private::ErrorCode::
        kLocalInvalidInvocationSource:
      return GlicPrivateApiStatusCodeHistogramValue::
          kLocalInvalidInvocationSource;
    case extensions::api::glic_private::ErrorCode::kLocalMissingPromptId:
      return GlicPrivateApiStatusCodeHistogramValue::kLocalMissingPromptId;
    case extensions::api::glic_private::ErrorCode::kServerMissingPrompt:
      return GlicPrivateApiStatusCodeHistogramValue::kServerMissingPrompt;
    case extensions::api::glic_private::ErrorCode::kHttpError:
      return GlicPrivateApiStatusCodeHistogramValue::kHttpError;
    case extensions::api::glic_private::ErrorCode::kParseError:
      return GlicPrivateApiStatusCodeHistogramValue::kParseError;
    case extensions::api::glic_private::ErrorCode::kLocalNoActiveTab:
      return GlicPrivateApiStatusCodeHistogramValue::kLocalNoActiveTab;
    case extensions::api::glic_private::ErrorCode::kLocalGlicNotEnabled:
      return GlicPrivateApiStatusCodeHistogramValue::kLocalGlicNotEnabled;
    case extensions::api::glic_private::ErrorCode::kLocalGlicNotReady:
      return GlicPrivateApiStatusCodeHistogramValue::kLocalGlicNotReady;
    case extensions::api::glic_private::ErrorCode::
        kLocalGlicActuationNotAllowed:
      return GlicPrivateApiStatusCodeHistogramValue::
          kLocalGlicActuationNotAllowed;
    case extensions::api::glic_private::ErrorCode::
        kLocalGlicNotEnabledAndConsented:
      return GlicPrivateApiStatusCodeHistogramValue::
          kLocalGlicNotEnabledAndConsented;
  }
}

api::glic_private::ProfileReadyState ConvertProfileReadyState(
    glic::mojom::ProfileReadyState state) {
  switch (state) {
    case glic::mojom::ProfileReadyState::kUnknownError:
      return api::glic_private::ProfileReadyState::kError;
    case glic::mojom::ProfileReadyState::kSignInRequired:
      return api::glic_private::ProfileReadyState::kSignInRequired;
    case glic::mojom::ProfileReadyState::kReady:
      return api::glic_private::ProfileReadyState::kReady;
    case glic::mojom::ProfileReadyState::kIneligible:
      return api::glic_private::ProfileReadyState::kIneligible;
    case glic::mojom::ProfileReadyState::kDisabledByAdmin:
      return api::glic_private::ProfileReadyState::kDisabledByAdmin;
  }
}

api::glic_private::ProfileState CreateProfileState(Profile* profile) {
  api::glic_private::ProfileState state;

  glic::GlicEnabling::ProfileEnablement enablement =
      glic::GlicEnabling::EnablementForProfile(profile);

  state.is_enabled = enablement.IsEnabled();
  state.is_enabled_and_consented = enablement.IsEnabledAndConsented();
  state.ready_state = ConvertProfileReadyState(
      glic::GlicEnabling::GetProfileReadyState(profile));

  state.live_allowed = enablement.EligibleForLive();
  state.share_image_allowed = enablement.EligibleForShareImage();

  glic::GlicKeyedService* glic_service = glic::GlicKeyedService::Get(profile);

  state.actuation_allowed =
      base::FeatureList::IsEnabled(features::kGlicActor) && glic_service &&
      glic_service->actor_policy_checker().CanActOnWeb();

  return state;
}

void OnEndpointFetcherResponse(
    PromptCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (response->error_type || response->http_status_code != 200) {
    std::move(callback).Run(
        extensions::api::glic_private::ErrorCode::kHttpError, std::nullopt);
    return;
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(response->response, 0);
  if (!value || !value->is_dict()) {
    std::move(callback).Run(
        extensions::api::glic_private::ErrorCode::kParseError, std::nullopt);
    return;
  }

  const std::string* prompt = value->GetDict().FindString(kPrompt);

  extensions::api::glic_private::ErrorCode result =
      extensions::api::glic_private::ErrorCode::kNone;

  if (!prompt || prompt->empty()) {
    result = extensions::api::glic_private::ErrorCode::kServerMissingPrompt;
  }

  std::move(callback).Run(result,
                          prompt ? std::make_optional(*prompt) : std::nullopt);
}

void GetPromptFromId(Profile& profile,
                     const std::string& prompt_id,
                     const std::string& invocation_source,
                     PromptCallback callback) {
  // Prompt fetching is not supported in incognito profiles.
  CHECK(!profile.IsOffTheRecord());

  base::Value request_dict(base::Value::Type::DICT);
  request_dict.GetDict().Set(kPromptId, prompt_id);
  request_dict.GetDict().Set(kInvocationSource, invocation_source);

  std::string post_data;
  base::JSONWriter::Write(request_dict, &post_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("glic_private_api_prompt_fetcher", R"(
        semantics {
          sender: "Gemini in Chrome Private API"
          description:
            "Fetches prompts from the server endpoint by prompt ID."
            "The prompts are used by Google websites."
          trigger:
            "When glicPrivate.invoke is called by the Gemini in Chrome"
            "internal extension triggered by Google websites."
          data: "Prompt ID and invocation source."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2026-04-10"
        }
        policy {
          cookies_allowed: NO
          setting: "Can be disabled by disabling Gemini in Chrome settings."
          policy_exception_justification: "Not implemented"
        }
      )");

  auto fetcher = std::make_unique<endpoint_fetcher::EndpointFetcher>(
      profile.GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      IdentityManagerFactory::GetForProfile(&profile),
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kPost, traffic_annotation)
          .SetUrl(GURL(extensions_features::kProdPromptEndpointUrlParam.Get()))
          .SetAuthType(endpoint_fetcher::AuthType::OAUTH)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kGlicInvokeApi)
          .SetContentType("application/json")
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .SetPostData(post_data)
          .Build());

  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherResponse,
                                    std::move(callback), std::move(fetcher)));
}

}  // namespace

GlicPrivateGetStateFunction::GlicPrivateGetStateFunction() = default;
GlicPrivateGetStateFunction::~GlicPrivateGetStateFunction() = default;

ExtensionFunction::ResponseAction GlicPrivateGetStateFunction::Run() {
  CHECK(base::FeatureList::IsEnabled(extensions_features::kApiGlicPrivate));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(ArgumentList(api::glic_private::GetState::Results::Create(
      CreateProfileState(profile))));
}

GlicPrivateInvokeFunction::GlicPrivateInvokeFunction() = default;
GlicPrivateInvokeFunction::~GlicPrivateInvokeFunction() = default;

ExtensionFunction::ResponseValue
GlicPrivateInvokeFunction::GetPromptResponseValueAndLog(
    extensions::api::glic_private::ErrorCode result) {
  GlicPrivateApiStatusCodeHistogramValue histogram_value =
      ConvertStatusCodeToHistogramValue(result);

  base::UmaHistogramEnumeration("Glic.PrivateApi.PromptRetrievalResult",
                                histogram_value);

  if (result != extensions::api::glic_private::ErrorCode::kNone) {
    return Error(api::glic_private::ToString(result));
  }

  return NoArguments();
}

ExtensionFunction::ResponseAction GlicPrivateInvokeFunction::Run() {
  std::optional<api::glic_private::Invoke::Params> params =
      api::glic_private::Invoke::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  api::glic_private::ProfileState profile_state = CreateProfileState(profile);
  if (!profile_state.is_enabled) {
    return RespondNow(GetPromptResponseValueAndLog(
        extensions::api::glic_private::ErrorCode::kLocalGlicNotEnabled));
  }
  if (profile_state.ready_state !=
      api::glic_private::ProfileReadyState::kReady) {
    return RespondNow(GetPromptResponseValueAndLog(
        extensions::api::glic_private::ErrorCode::kLocalGlicNotReady));
  }
  if (!profile_state.actuation_allowed) {
    return RespondNow(
        GetPromptResponseValueAndLog(extensions::api::glic_private::ErrorCode::
                                         kLocalGlicActuationNotAllowed));
  }
  if (extensions_features::kGlicRequireConsentForInvokeParam.Get() &&
      !profile_state.is_enabled_and_consented) {
    return RespondNow(
        GetPromptResponseValueAndLog(extensions::api::glic_private::ErrorCode::
                                         kLocalGlicNotEnabledAndConsented));
  }

  glic::mojom::InvocationSource source =
      glic::mojom::InvocationSource::kUnsupported;
  glic::mojom::FeatureMode feature_mode =
      glic::mojom::FeatureMode::kUnspecified;
  switch (params->details.invocation_source) {
    case api::glic_private::InvocationSource::kUniversalCart:
      if (!base::FeatureList::IsEnabled(
              extensions_features::kApiGlicAccessFromGoogleWebpage)) {
        return RespondNow(GetPromptResponseValueAndLog(
            extensions::api::glic_private::ErrorCode::kLocalGlicNotEnabled));
      }
      source = glic::mojom::InvocationSource::kUniversalCart;
      feature_mode = glic::mojom::FeatureMode::kBluedog;
      break;
    default:
      return RespondNow(GetPromptResponseValueAndLog(
          extensions::api::glic_private::ErrorCode::
              kLocalInvalidInvocationSource));
  }

  if (params->details.prompt_id.empty()) {
    return RespondNow(GetPromptResponseValueAndLog(
        extensions::api::glic_private::ErrorCode::kLocalMissingPromptId));
  }

  glic::GlicInvokeOptions options{source};

  options.feature_mode = feature_mode;
  options.target.conversation = glic::NewConversation();

  bool in_new_tab = params->details.in_new_tab.value_or(false);

  GetPromptFromId(*profile, params->details.prompt_id,
                  InvocationSourceToString(params->details.invocation_source),
                  base::BindOnce(&GlicPrivateInvokeFunction::OnPromptRetrieved,
                                 this, std::move(options), in_new_tab));

  return RespondLater();
}

void GlicPrivateInvokeFunction::OnPromptRetrieved(
    glic::GlicInvokeOptions options,
    bool in_new_tab,
    extensions::api::glic_private::ErrorCode result,
    std::optional<std::string> prompt) {
  if (!browser_context()) {
    return;
  }

  if (result != extensions::api::glic_private::ErrorCode::kNone) {
    Respond(GetPromptResponseValueAndLog(result));
    return;
  }

  options.prompts.push_back(std::move(*prompt));

  Profile* profile = Profile::FromBrowserContext(browser_context());

  tabs::TabInterface* tab_interface = nullptr;

  if (in_new_tab) {
    // Navigate to a new tab.
    NavigateParams navigate_params(profile, GURL("about:blank"),
                                   ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    base::WeakPtr<content::NavigationHandle> navigation_handle =
        Navigate(&navigate_params);
    if (navigation_handle) {
      tab_interface = tabs::TabInterface::MaybeGetFromContents(
          navigation_handle->GetWebContents());
    }
  } else {
    // Find the active tab.
    // TODO(b/497936770): Find the tab from the caller. Make sure we actually
    // need it before implement.
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == profile) {
            tab_interface = TabListInterface::From(browser)->GetActiveTab();
            return false;  // Stop iterating.
          }
          return true;  // Continue iterating.
        });
  }

  if (!tab_interface) {
    Respond(GetPromptResponseValueAndLog(
        extensions::api::glic_private::ErrorCode::kLocalNoActiveTab));
    return;
  }

  options.target.surface = tab_interface;

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile,
                                                         /*create=*/true);
  CHECK(glic_service);

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options));

  Respond(GetPromptResponseValueAndLog(
      extensions::api::glic_private::ErrorCode::kNone));
}

}  // namespace extensions

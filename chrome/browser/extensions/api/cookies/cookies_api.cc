// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/cookies/cookies_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/common/extensions/api/cookies.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/stack_frame.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/mojom/network_service.mojom.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Keys
constexpr char kCauseKey[] = "cause";
constexpr char kCookieKey[] = "cookie";
constexpr char kRemovedKey[] = "removed";

// Cause Constants
constexpr char kEvictedChangeCause[] = "evicted";
constexpr char kExpiredChangeCause[] = "expired";
constexpr char kExpiredOverwriteChangeCause[] = "expired_overwrite";
constexpr char kExplicitChangeCause[] = "explicit";
constexpr char kOverwriteChangeCause[] = "overwrite";

// Errors
constexpr char kCookieSetFailedError[] =
    "Failed to parse or set cookie named \"*\".";
constexpr char kInvalidStoreIdError[] = "Invalid cookie store id: \"*\".";
constexpr char kInvalidUrlError[] = "Invalid url: \"*\".";
constexpr char kNoHostPermissionsError[] =
    "No host permissions for cookies at url: \"*\".";

bool ParseUrl(const Extension* extension,
              const std::string& url_string,
              GURL* url,
              bool check_host_permissions,
              std::string* error) {
  *url = GURL(url_string);
  if (!url->is_valid()) {
    *error = ErrorUtils::FormatErrorMessage(kInvalidUrlError, url_string);
    return false;
  }
  // Check against host permissions if needed.
  if (check_host_permissions &&
      !extension->permissions_data()->HasHostPermission(*url)) {
    *error =
        ErrorUtils::FormatErrorMessage(kNoHostPermissionsError, url->spec());
    return false;
  }
  return true;
}

network::mojom::CookieManager* ParseStoreCookieManager(
    content::BrowserContext* function_context,
    bool include_incognito,
    std::string* store_id,
    std::string* error) {
  Profile* function_profile = Profile::FromBrowserContext(function_context);
  Profile* store_profile = nullptr;
  if (!store_id->empty()) {
    store_profile = cookies_helpers::ChooseProfileFromStoreId(
        *store_id, function_profile, include_incognito);
    if (!store_profile) {
      *error = ErrorUtils::FormatErrorMessage(kInvalidStoreIdError, *store_id);
      return nullptr;
    }
  } else {
    store_profile = function_profile;
    *store_id = cookies_helpers::GetStoreIdFromProfile(store_profile);
  }

  return store_profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

}  // namespace

CookiesEventRouter::CookieChangeListener::CookieChangeListener(
    CookiesEventRouter* router,
    bool otr)
    : router_(router), otr_(otr) {}
CookiesEventRouter::CookieChangeListener::~CookieChangeListener() = default;

void CookiesEventRouter::CookieChangeListener::OnCookieChange(
    const net::CookieChangeInfo& change) {
  router_->OnCookieChange(otr_, change);
}

CookiesEventRouter::CookiesEventRouter(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      profile_observation_(this) {
  MaybeStartListening();
  profile_observation_.Observe(profile_);
}

CookiesEventRouter::~CookiesEventRouter() = default;

void CookiesEventRouter::OnCookieChange(bool otr,
                                        const net::CookieChangeInfo& change) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no way to represent non-serializable
  // partition keys in JS so return to prevent a crash.
  if (change.cookie.IsPartitioned() &&
      !change.cookie.PartitionKey()->IsSerializeable()) {
    return;
  }
  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(kRemovedKey, change.cause != net::CookieChangeCause::INSERTED);

  Profile* profile =
      otr ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : profile_->GetOriginalProfile();
  api::cookies::Cookie cookie = cookies_helpers::CreateCookie(
      change.cookie, cookies_helpers::GetStoreIdFromProfile(profile));
  dict.Set(kCookieKey, cookie.ToValue());

  // Map the internal cause to an external string.
  std::string cause_dict_entry;
  switch (change.cause) {
    // Report an inserted cookie as an "explicit" change cause. All other causes
    // only make sense for deletions.
    case net::CookieChangeCause::INSERTED:
    case net::CookieChangeCause::EXPLICIT:
      cause_dict_entry = kExplicitChangeCause;
      break;

    case net::CookieChangeCause::OVERWRITE:
      cause_dict_entry = kOverwriteChangeCause;
      break;

    case net::CookieChangeCause::EXPIRED:
      cause_dict_entry = kExpiredChangeCause;
      break;

    case net::CookieChangeCause::EVICTED:
      cause_dict_entry = kEvictedChangeCause;
      break;

    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      cause_dict_entry = kExpiredOverwriteChangeCause;
      break;

    case net::CookieChangeCause::UNKNOWN_DELETION:
      NOTREACHED_IN_MIGRATION();
  }
  dict.Set(kCauseKey, cause_dict_entry);

  args.Append(std::move(dict));

  DispatchEvent(profile, events::COOKIES_ON_CHANGED,
                api::cookies::OnChanged::kEventName, std::move(args),
                cookies_helpers::GetURLFromCanonicalCookie(change.cookie));
}

void CookiesEventRouter::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  // When an off-the-record spinoff of |profile_| is created, start listening
  // for cookie changes there. The OTR receiver should never be bound, since
  // there wasn't previously an OTR profile.
  if (!otr_receiver_.is_bound()) {
    BindToCookieManager(&otr_receiver_, off_the_record);
  }
}

void CookiesEventRouter::MaybeStartListening() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  Profile* original_profile = profile_->GetOriginalProfile();
  Profile* otr_profile =
      original_profile->HasPrimaryOTRProfile()
          ? original_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;

  if (!receiver_.is_bound())
    BindToCookieManager(&receiver_, original_profile);
  if (!otr_receiver_.is_bound() && otr_profile)
    BindToCookieManager(&otr_receiver_, otr_profile);
}

void CookiesEventRouter::BindToCookieManager(
    mojo::Receiver<network::mojom::CookieChangeListener>* receiver,
    Profile* profile) {
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  if (!cookie_manager)
    return;

  cookie_manager->AddGlobalChangeListener(receiver->BindNewPipeAndPassRemote());
  receiver->set_disconnect_handler(
      base::BindOnce(&CookiesEventRouter::OnConnectionError,
                     base::Unretained(this), receiver));
}

void CookiesEventRouter::OnConnectionError(
    mojo::Receiver<network::mojom::CookieChangeListener>* receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  receiver->reset();
  MaybeStartListening();
}

void CookiesEventRouter::DispatchEvent(content::BrowserContext* context,
                                       events::HistogramValue histogram_value,
                                       const std::string& event_name,
                                       base::Value::List event_args,
                                       const GURL& cookie_domain) {
  EventRouter* router = context ? EventRouter::Get(context) : nullptr;
  if (!router)
    return;
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), context);
  event->event_url = cookie_domain;
  router->BroadcastEvent(std::move(event));
}

CookiesGetFunction::CookiesGetFunction() = default;
CookiesGetFunction::~CookiesGetFunction() = default;

ExtensionFunction::ResponseAction CookiesGetFunction::Run() {
  parsed_args_ = api::cookies::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  // Read/validate input parameters.
  std::string error;
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error))
    return RespondNow(Error(std::move(error)));

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager)
    return RespondNow(Error(std::move(error)));

  if (parsed_args_->details.partition_key.has_value() &&
      !parsed_args_->details.partition_key->has_cross_site_ancestor
           .has_value() &&
      parsed_args_->details.partition_key->top_level_site.has_value()) {
    base::expected<bool, std::string> cross_site_ancestor =
        cookies_helpers::CalculateHasCrossSiteAncestor(
            parsed_args_->details.url, parsed_args_->details.partition_key);
    if (!cross_site_ancestor.has_value()) {
      return RespondNow(Error(std::move(cross_site_ancestor.error())));
    }
    parsed_args_->details.partition_key->has_cross_site_ancestor =
        cross_site_ancestor.value();
  }

  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = cookies_helpers::ToNetCookiePartitionKey(
          parsed_args_->details.partition_key);
  if (!partition_key.has_value()) {
    return RespondNow(Error(std::move(partition_key.error())));
  }

  if (!parsed_args_->details.store_id)
    parsed_args_->details.store_id = store_id;

  DCHECK(!url_.is_empty() && url_.is_valid());
  cookies_helpers::GetCookieListFromManager(
      cookie_manager, url_,
      net::CookiePartitionKeyCollection::FromOptional(partition_key.value()),
      base::BindOnce(&CookiesGetFunction::GetCookieListCallback, this));

  // Extension telemetry signal intercept
  NotifyExtensionTelemetry();

  // Will finish asynchronously.
  return RespondLater();
}

void CookiesGetFunction::GetCookieListCallback(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const net::CookieWithAccessResult& cookie_with_access_result :
       cookie_list) {
    if (!cookies_helpers::
            CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
                parsed_args_->details.partition_key,
                cookie_with_access_result.cookie.PartitionKey())) {
      continue;
    }

    // Return the first matching cookie. Relies on the fact that the
    // CookieManager interface returns them in canonical order (longest path,
    // then earliest creation time).
    if (cookie_with_access_result.cookie.Name() == parsed_args_->details.name) {
      api::cookies::Cookie api_cookie = cookies_helpers::CreateCookie(
          cookie_with_access_result.cookie, *parsed_args_->details.store_id);
      Respond(ArgumentList(api::cookies::Get::Results::Create(api_cookie)));
      return;
    }
  }

  // The cookie doesn't exist; return null.
  Respond(WithArguments(base::Value()));
}

void CookiesGetFunction::NotifyExtensionTelemetry() {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  auto cookies_get_signal = std::make_unique<safe_browsing::CookiesGetSignal>(
      extension_id(), parsed_args_->details.name,
      parsed_args_->details.store_id.value_or(std::string()),
      parsed_args_->details.url, js_callstack().value_or(StackTrace()));
  telemetry_service->AddSignal(std::move(cookies_get_signal));
}

CookiesGetAllFunction::CookiesGetAllFunction() {
}

CookiesGetAllFunction::~CookiesGetAllFunction() {
}

ExtensionFunction::ResponseAction CookiesGetAllFunction::Run() {
  parsed_args_ = api::cookies::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  std::string error;
  if (parsed_args_->details.url &&
      !ParseUrl(extension(), *parsed_args_->details.url, &url_, false,
                &error)) {
    return RespondNow(Error(std::move(error)));
  }

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager)
    return RespondNow(Error(std::move(error)));

  // make sure user input is valid
  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = cookies_helpers::ToNetCookiePartitionKey(
          parsed_args_->details.partition_key);
  if (!partition_key.has_value()) {
    return RespondNow(Error(std::move(partition_key.error())));
  }

  if (!parsed_args_->details.store_id)
    parsed_args_->details.store_id = store_id;

  net::CookiePartitionKeyCollection cookie_partition_key_collection =
      cookies_helpers::CookiePartitionKeyCollectionFromApiPartitionKey(
          parsed_args_->details.partition_key);

  DCHECK(url_.is_empty() || url_.is_valid());
  if (url_.is_empty()) {
    cookies_helpers::GetAllCookiesFromManager(
        cookie_manager,
        base::BindOnce(&CookiesGetAllFunction::GetAllCookiesCallback, this));
  } else {
    cookies_helpers::GetCookieListFromManager(
        cookie_manager, url_, cookie_partition_key_collection,
        base::BindOnce(&CookiesGetAllFunction::GetCookieListCallback, this));
  }

  // Extension telemetry signal intercept
  NotifyExtensionTelemetry();

  return RespondLater();
}

void CookiesGetAllFunction::GetAllCookiesCallback(
    const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (extension()) {
    net::CookiePartitionKeyCollection cookie_partition_key_collection =
        cookies_helpers::CookiePartitionKeyCollectionFromApiPartitionKey(
            parsed_args_->details.partition_key);
    std::vector<api::cookies::Cookie> match_vector;
    cookies_helpers::AppendMatchingCookiesFromCookieListToVector(
        cookie_list, &parsed_args_->details, extension(), &match_vector,
        cookie_partition_key_collection);

    Respond(ArgumentList(api::cookies::GetAll::Results::Create(match_vector)));
  } else {
    // TODO(devlin): When can |extension()| be null for this function?
    Respond(NoArguments());
  }
}

void CookiesGetAllFunction::GetCookieListCallback(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (extension()) {
    std::vector<api::cookies::Cookie> match_vector;
    cookies_helpers::AppendMatchingCookiesFromCookieAccessResultListToVector(
        cookie_list, &parsed_args_->details, extension(), &match_vector);

    Respond(ArgumentList(api::cookies::GetAll::Results::Create(match_vector)));
  } else {
    // TODO(devlin): When can |extension()| be null for this function?
    Respond(NoArguments());
  }
}

void CookiesGetAllFunction::NotifyExtensionTelemetry() {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  auto cookies_get_all_signal =
      std::make_unique<safe_browsing::CookiesGetAllSignal>(
          extension_id(), parsed_args_->details.domain.value_or(std::string()),
          parsed_args_->details.name.value_or(std::string()),
          parsed_args_->details.path.value_or(std::string()),
          parsed_args_->details.secure,
          parsed_args_->details.store_id.value_or(std::string()),
          parsed_args_->details.url.value_or(std::string()),
          parsed_args_->details.session, js_callstack().value_or(StackTrace()));
  telemetry_service->AddSignal(std::move(cookies_get_all_signal));
}

CookiesSetFunction::CookiesSetFunction()
    : state_(NO_RESPONSE), success_(false) {}

CookiesSetFunction::~CookiesSetFunction() {
}

ExtensionFunction::ResponseAction CookiesSetFunction::Run() {
  parsed_args_ = api::cookies::Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  // Read/validate input parameters.
  std::string error;
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error))
    return RespondNow(Error(std::move(error)));

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager)
    return RespondNow(Error(std::move(error)));

  // cookies.set api allows for an partitionKey with a `top_level_site` present
  // but no value for `has_cross_site_ancestor`. If that is the case, the
  // browser will calculate the value for `has_cross_site_ancestor`.
  std::optional<extensions::api::cookies::CookiePartitionKey> api_partition_key;
  if (parsed_args_->details.partition_key.has_value()) {
    api_partition_key = parsed_args_->details.partition_key->Clone();
    if (!api_partition_key->has_cross_site_ancestor.has_value() &&
        api_partition_key->top_level_site.has_value()) {
      base::expected<bool, std::string> cross_site_ancestor =
          cookies_helpers::CalculateHasCrossSiteAncestor(
              parsed_args_->details.url, api_partition_key);
      if (!cross_site_ancestor.has_value()) {
        return RespondNow(Error(std::move(cross_site_ancestor.error())));
      }
      api_partition_key->has_cross_site_ancestor = cross_site_ancestor.value();
    }
  }

  if (!cookies_helpers::ValidateCrossSiteAncestor(parsed_args_->details.url,
                                                  api_partition_key, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      net_partition_key =
          cookies_helpers::ToNetCookiePartitionKey(api_partition_key);
  if (!net_partition_key.has_value()) {
    return RespondNow(Error(std::move(net_partition_key.error())));
  }

  if (!parsed_args_->details.store_id)
    parsed_args_->details.store_id = store_id;

  base::Time expiration_time;
  if (parsed_args_->details.expiration_date) {
    // Time::FromSecondsSinceUnixEpoch converts double time 0 to empty Time
    // object. So we need to do special handling here.
    expiration_time = (*parsed_args_->details.expiration_date == 0)
                          ? base::Time::UnixEpoch()
                          : base::Time::FromSecondsSinceUnixEpoch(
                                *parsed_args_->details.expiration_date);
  }

  net::CookieSameSite same_site = net::CookieSameSite::UNSPECIFIED;
  switch (parsed_args_->details.same_site) {
    case api::cookies::SameSiteStatus::kNoRestriction:
      same_site = net::CookieSameSite::NO_RESTRICTION;
      break;
    case api::cookies::SameSiteStatus::kLax:
      same_site = net::CookieSameSite::LAX_MODE;
      break;
    case api::cookies::SameSiteStatus::kStrict:
      same_site = net::CookieSameSite::STRICT_MODE;
      break;
    // This is the case if the optional sameSite property is given as
    // "unspecified":
    case api::cookies::SameSiteStatus::kUnspecified:
    // This is the case if the optional sameSite property is left out:
    case api::cookies::SameSiteStatus::kNone:
      same_site = net::CookieSameSite::UNSPECIFIED;
      break;
  }

  std::unique_ptr<net::CanonicalCookie> cc(
      net::CanonicalCookie::CreateSanitizedCookie(
          url_,                                                  //
          parsed_args_->details.name.value_or(std::string()),    //
          parsed_args_->details.value.value_or(std::string()),   //
          parsed_args_->details.domain.value_or(std::string()),  //
          parsed_args_->details.path.value_or(std::string()),    //
          /*creation_time=*/base::Time(),                        //
          expiration_time,                                       //
          /*last_access_time=*/base::Time(),                     //
          parsed_args_->details.secure.value_or(false),          //
          parsed_args_->details.http_only.value_or(false),       //
          same_site,                                             //
          net::COOKIE_PRIORITY_DEFAULT,                          //
          net_partition_key.value(),                             //
          /*status=*/nullptr));
  if (!cc) {
    // Return error through callbacks so that the proper error message
    // is generated.
    success_ = false;
    state_ = SET_COMPLETED;
    GetCookieListCallback(net::CookieAccessResultList(),
                          net::CookieAccessResultList());
    return AlreadyResponded();
  }

  // Dispatch the setter, immediately followed by the getter.  This
  // plus FIFO ordering on the cookie_manager_ pipe means that no
  // other extension function will affect the get result.
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  DCHECK(!url_.is_empty() && url_.is_valid());
  cookie_manager->SetCanonicalCookie(
      *cc, url_, options,
      base::BindOnce(&CookiesSetFunction::SetCanonicalCookieCallback, this));
  cookies_helpers::GetCookieListFromManager(
      cookie_manager, url_,
      net::CookiePartitionKeyCollection::FromOptional(
          net_partition_key.value()),
      base::BindOnce(&CookiesSetFunction::GetCookieListCallback, this));

  // Will finish asynchronously.
  return RespondLater();
}

void CookiesSetFunction::SetCanonicalCookieCallback(
    net::CookieAccessResult set_cookie_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(NO_RESPONSE, state_);
  state_ = SET_COMPLETED;
  success_ = set_cookie_result.status.IsInclude();
}

void CookiesSetFunction::GetCookieListCallback(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(SET_COMPLETED, state_);
  state_ = GET_COMPLETED;

  if (!success_) {
    std::string name = parsed_args_->details.name.value_or(std::string());
    Respond(Error(ErrorUtils::FormatErrorMessage(kCookieSetFailedError, name)));
    return;
  }

  std::optional<ResponseValue> value;
  for (const net::CookieWithAccessResult& cookie_with_access_result :
       cookie_list) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster returns them in canonical order (longest path, then
    // earliest creation time).

    if (!extensions::cookies_helpers::
            CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
                parsed_args_->details.partition_key,
                cookie_with_access_result.cookie.PartitionKey())) {
      continue;
    }

    std::string name = parsed_args_->details.name.value_or(std::string());

    if (cookie_with_access_result.cookie.Name() == name) {
      api::cookies::Cookie api_cookie = cookies_helpers::CreateCookie(
          cookie_with_access_result.cookie, *parsed_args_->details.store_id);
      value.emplace(
          ArgumentList(api::cookies::Set::Results::Create(api_cookie)));
      break;
    }
  }

  Respond(value ? std::move(*value) : NoArguments());
}

CookiesRemoveFunction::CookiesRemoveFunction() {
}

CookiesRemoveFunction::~CookiesRemoveFunction() {
}

ExtensionFunction::ResponseAction CookiesRemoveFunction::Run() {
  parsed_args_ = api::cookies::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  // Read/validate input parameters.
  std::string error;
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error))
    return RespondNow(Error(std::move(error)));

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager)
    return RespondNow(Error(std::move(error)));

  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = cookies_helpers::ToNetCookiePartitionKey(
          parsed_args_->details.partition_key);
  if (!partition_key.has_value()) {
    return RespondNow(Error(std::move(partition_key.error())));
  }

  if (!parsed_args_->details.store_id)
    parsed_args_->details.store_id = store_id;

  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());

  filter->cookie_partition_key_collection =
      net::CookiePartitionKeyCollection::FromOptional(partition_key.value());
  filter->url = url_;
  filter->cookie_name = parsed_args_->details.name;
  cookie_manager->DeleteCookies(
      std::move(filter),
      base::BindOnce(&CookiesRemoveFunction::RemoveCookieCallback, this));

  // Will return asynchronously.
  return RespondLater();
}

void CookiesRemoveFunction::RemoveCookieCallback(uint32_t /* num_deleted */) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Build the callback result
  api::cookies::Remove::Results::Details details;
  details.name = parsed_args_->details.name;
  details.url = url_.spec();
  details.store_id = *parsed_args_->details.store_id;
  if (parsed_args_->details.partition_key) {
    details.partition_key = parsed_args_->details.partition_key->Clone();
  }

  Respond(ArgumentList(api::cookies::Remove::Results::Create(details)));
}

ExtensionFunction::ResponseAction CookiesGetAllCookieStoresFunction::Run() {
  Profile* original_profile = Profile::FromBrowserContext(browser_context());
  DCHECK(original_profile);
  base::Value::List original_tab_ids;
  Profile* incognito_profile = nullptr;
  base::Value::List incognito_tab_ids;
  if (include_incognito_information() &&
      original_profile->HasPrimaryOTRProfile()) {
    incognito_profile =
        original_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  DCHECK(original_profile != incognito_profile);

  // Iterate through all browser instances, and for each browser,
  // add its tab IDs to either the regular or incognito tab ID list depending
  // whether the browser is regular or incognito.
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    if (window->profile() == original_profile) {
      cookies_helpers::AppendToTabIdList(window, original_tab_ids);
    } else if (window->profile() == incognito_profile) {
      cookies_helpers::AppendToTabIdList(window, incognito_tab_ids);
    }
  }
  // Return a list of all cookie stores with at least one open tab.
  std::vector<api::cookies::CookieStore> cookie_stores;
  if (!original_tab_ids.empty()) {
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        original_profile, std::move(original_tab_ids)));
  }
  if (incognito_profile && !incognito_tab_ids.empty()) {
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        incognito_profile, std::move(incognito_tab_ids)));
  }
  return RespondNow(ArgumentList(
      api::cookies::GetAllCookieStores::Results::Create(cookie_stores)));
}

CookiesAPI::CookiesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, api::cookies::OnChanged::kEventName);
}

CookiesAPI::~CookiesAPI() = default;

void CookiesAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CookiesAPI>>::
    DestructorAtExit g_cookies_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CookiesAPI>* CookiesAPI::GetFactoryInstance() {
  return g_cookies_api_factory.Pointer();
}

void CookiesAPI::OnListenerAdded(const EventListenerInfo& details) {
  cookies_event_router_ =
      std::make_unique<CookiesEventRouter>(browser_context_);
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions

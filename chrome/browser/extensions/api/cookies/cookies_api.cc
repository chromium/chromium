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
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/api/cookies.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/cookies/cookies_api_delegate.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/browser_context_lifetime_tracker.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/safe_browsing_delegate.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/stack_frame.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

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

bool CheckHostPermissions(const Extension* extension,
                          const GURL& url,
                          std::string* error) {
  // Cookie operations are profile-scoped and not tied to a specific tab
  // context so we pass kUnknownTabId to check page access without considering
  // tab-specific grants.
  if (extension->permissions_data()->GetPageAccess(
          url, extension_misc::kUnknownTabId, /*error=*/nullptr) !=
      PermissionsData::PageAccess::kAllowed) {
    if (error) {
      *error =
          ErrorUtils::FormatErrorMessage(kNoHostPermissionsError, url.spec());
    }
    return false;
  }
  return true;
}

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
  if (check_host_permissions && !CheckHostPermissions(extension, *url, error)) {
    return false;
  }
  return true;
}

network::mojom::CookieManager* ParseStoreCookieManager(
    content::BrowserContext* function_context,
    bool include_incognito,
    std::string* store_id,
    std::string* error) {
  content::BrowserContext* store_context = nullptr;
  if (!store_id->empty()) {
    store_context = cookies_helpers::ChooseBrowserContextFromStoreId(
        *store_id, function_context, include_incognito);
    if (!store_context) {
      *error = ErrorUtils::FormatErrorMessage(kInvalidStoreIdError, *store_id);
      return nullptr;
    }
  } else {
    store_context = function_context;
    *store_id = cookies_helpers::GetStoreIdFromBrowserContext(store_context);
  }

  return store_context->GetDefaultStoragePartition()
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
    : browser_context_(context),
      browser_context_lifetime_tracker_(
          ExtensionsBrowserClient::Get()->GetBrowserContextLifetimeTracker()) {
  MaybeStartListening();
  if (browser_context_lifetime_tracker_) {
    browser_context_lifetime_tracker_->StartObserving(*browser_context_, *this);
  }
}

CookiesEventRouter::~CookiesEventRouter() {
  if (browser_context_lifetime_tracker_) {
    browser_context_lifetime_tracker_->StopObserving(*this);
  }
}

void CookiesEventRouter::OnCookieChange(bool otr,
                                        const net::CookieChangeInfo& change) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no way to represent non-serializable
  // partition keys in JS so return to prevent a crash.
  if (change.cookie.IsPartitioned() &&
      !change.cookie.PartitionKey()->IsSerializeable()) {
    return;
  }
  base::ListValue args;
  base::DictValue dict;
  dict.Set(kRemovedKey,
           change.cause != net::CookieChangeCause::INSERTED &&
               change.cause !=
                   net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE &&
               change.cause !=
                   net::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE);

  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  content::BrowserContext* context =
      otr ? (client->HasOffTheRecordContext(browser_context_)
                 ? client->GetOffTheRecordContext(browser_context_)
                 : nullptr)
          : client->GetOriginalContext(browser_context_);
  // TODO(407373848): OTR profile must exist when the cookie change event
  // arrived.
  CHECK(context);

  api::cookies::Cookie cookie = cookies_helpers::CreateCookie(
      change.cookie, cookies_helpers::GetStoreIdFromBrowserContext(context));
  dict.Set(kCookieKey, cookie.ToValue());

  // Map the internal cause to an external string.
  std::string cause_dict_entry;
  switch (change.cause) {
    // Report an inserted cookie as an "explicit" change cause. All other causes
    // only make sense for deletions.
    case net::CookieChangeCause::INSERTED:
    case net::CookieChangeCause::EXPLICIT:
    case net::CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE:
    case net::CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE:
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
      NOTREACHED();
  }
  dict.Set(kCauseKey, cause_dict_entry);

  args.Append(std::move(dict));

  DispatchEvent(context, events::COOKIES_ON_CHANGED,
                api::cookies::OnChanged::kEventName, std::move(args),
                cookies_helpers::GetURLFromCanonicalCookie(change.cookie));
}

void CookiesEventRouter::OnRelatedOffTheRecordBrowserContextCreated(
    content::BrowserContext& off_the_record_context) {
  // Start listening for cookie changes there. The OTR receiver may already
  // be bound if MaybeStartListening() raced this callback (e.g. the
  // off-the-record context already existed when StartObserving() was
  // called).
  if (!otr_receiver_.is_bound()) {
    BindToCookieManager(&otr_receiver_, off_the_record_context);
  }
}

void CookiesEventRouter::OnRelatedOffTheRecordBrowserContextDestroyed(
    content::BrowserContext& off_the_record_context) {
  otr_receiver_.reset();
}

void CookiesEventRouter::MaybeStartListening() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context_);

  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  content::BrowserContext* original_context =
      client->GetOriginalContext(browser_context_);

  if (!receiver_.is_bound()) {
    BindToCookieManager(&receiver_, *original_context);
  }

  if (!otr_receiver_.is_bound() &&
      client->HasOffTheRecordContext(original_context)) {
    BindToCookieManager(&otr_receiver_,
                        *client->GetOffTheRecordContext(original_context));
  }
}

void CookiesEventRouter::BindToCookieManager(
    mojo::Receiver<network::mojom::CookieChangeListener>* receiver,
    content::BrowserContext& context) {
  network::mojom::CookieManager* cookie_manager =
      context.GetDefaultStoragePartition()->GetCookieManagerForBrowserProcess();
  if (!cookie_manager) {
    return;
  }

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
                                       base::ListValue event_args,
                                       const GURL& cookie_domain) {
  EventRouter* router = context ? EventRouter::Get(context) : nullptr;
  if (!router) {
    return;
  }
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
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager) {
    return RespondNow(Error(std::move(error)));
  }

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

  if (!parsed_args_->details.store_id) {
    parsed_args_->details.store_id = store_id;
  }

  DCHECK(!url_.is_empty() && url_.is_valid());
  cookies_helpers::GetCookieListFromManager(
      cookie_manager, url_,
      net::CookiePartitionKeyCollection(std::move(partition_key).value()),
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
  // TODO(crbug.com/371423073): Support telemetry on Android.
  ExtensionsBrowserClient::Get()
      ->GetSafeBrowsingDelegate()
      ->NotifyExtensionApiCookiesGet(
          browser_context(), extension_id(), parsed_args_->details.name,
          parsed_args_->details.store_id.value_or(std::string()),
          parsed_args_->details.url, js_callstack().value_or(StackTrace()));
}

CookiesGetAllFunction::CookiesGetAllFunction() = default;

CookiesGetAllFunction::~CookiesGetAllFunction() = default;

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
  if (!cookie_manager) {
    return RespondNow(Error(std::move(error)));
  }

  // make sure user input is valid
  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = cookies_helpers::ToNetCookiePartitionKey(
          parsed_args_->details.partition_key);
  if (!partition_key.has_value()) {
    return RespondNow(Error(std::move(partition_key.error())));
  }

  if (!parsed_args_->details.store_id) {
    parsed_args_->details.store_id = store_id;
  }

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
  ExtensionsBrowserClient::Get()
      ->GetSafeBrowsingDelegate()
      ->NotifyExtensionApiCookiesGetAll(
          browser_context(), extension_id(),
          parsed_args_->details.domain.value_or(std::string()),
          parsed_args_->details.name.value_or(std::string()),
          parsed_args_->details.path.value_or(std::string()),
          parsed_args_->details.secure,
          parsed_args_->details.store_id.value_or(std::string()),
          parsed_args_->details.url.value_or(std::string()),
          parsed_args_->details.session, js_callstack().value_or(StackTrace()));
}

CookiesSetFunction::CookiesSetFunction()
    : state_(NO_RESPONSE), success_(false) {}

CookiesSetFunction::~CookiesSetFunction() = default;

ExtensionFunction::ResponseAction CookiesSetFunction::Run() {
  parsed_args_ = api::cookies::Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  // Read/validate input parameters.
  std::string error;
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager) {
    return RespondNow(Error(std::move(error)));
  }

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

  if (!parsed_args_->details.store_id) {
    parsed_args_->details.store_id = store_id;
  }

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
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&CookiesSetFunction::SetCanonicalCookieCallback, this),
          net::CookieAccessResult()));
  cookies_helpers::GetCookieListFromManager(
      cookie_manager, url_,
      net::CookiePartitionKeyCollection(std::move(net_partition_key).value()),
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

CookiesRemoveFunction::CookiesRemoveFunction() = default;

CookiesRemoveFunction::~CookiesRemoveFunction() = default;

ExtensionFunction::ResponseAction CookiesRemoveFunction::Run() {
  parsed_args_ = api::cookies::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  // Read/validate input parameters.
  std::string error;
  if (!ParseUrl(extension(), parsed_args_->details.url, &url_, true, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  std::string store_id = parsed_args_->details.store_id.value_or(std::string());
  network::mojom::CookieManager* cookie_manager = ParseStoreCookieManager(
      browser_context(), include_incognito_information(), &store_id, &error);
  if (!cookie_manager) {
    return RespondNow(Error(std::move(error)));
  }

  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = cookies_helpers::ToNetCookiePartitionKey(
          parsed_args_->details.partition_key);
  if (!partition_key.has_value()) {
    return RespondNow(Error(std::move(partition_key.error())));
  }

  if (!parsed_args_->details.store_id) {
    parsed_args_->details.store_id = store_id;
  }

  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());

  filter->cookie_partition_key_collection =
      net::CookiePartitionKeyCollection(std::move(partition_key).value());
  filter->url = url_;
  filter->cookie_name = parsed_args_->details.name;
  cookie_manager->DeleteCookies(
      std::move(filter),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&CookiesRemoveFunction::RemoveCookieCallback, this),
          0u));

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

CookiesGetPartitionKeyFunction::CookiesGetPartitionKeyFunction() = default;

CookiesGetPartitionKeyFunction::~CookiesGetPartitionKeyFunction() = default;

ExtensionFunction::ResponseAction CookiesGetPartitionKeyFunction::Run() {
  parsed_args_ = api::cookies::GetPartitionKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parsed_args_);

  content::RenderFrameHost* render_frame_host = nullptr;
  content::WebContents* web_contents = nullptr;
  std::optional<int> frame_id = parsed_args_->details.frame_id;
  std::optional<int> tab_id = parsed_args_->details.tab_id;

  if (parsed_args_->details.document_id.has_value()) {
    ExtensionApiFrameIdMap::DocumentId document_id =
        ExtensionApiFrameIdMap::DocumentIdFromString(
            *parsed_args_->details.document_id);
    if (!document_id) {
      return RespondNow(Error("Invalid `documentId`."));
    }
    render_frame_host =
        ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
            document_id);
    if (!render_frame_host) {
      return RespondNow(Error("Invalid `documentId`."));
    }
    web_contents = content::WebContents::FromRenderFrameHost(render_frame_host);
    if (!web_contents ||
        !util::IsWebContentsInContext(*web_contents, *browser_context(),
                                      include_incognito_information())) {
      return RespondNow(Error("Invalid `documentId`."));
    }

    if ((tab_id.has_value() &&
         ExtensionTabUtil::GetTabId(web_contents) != tab_id.value()) ||
        (frame_id.has_value() && ExtensionApiFrameIdMap::GetFrameId(
                                     render_frame_host) != frame_id.value())) {
      return RespondNow(
          Error("Provided `tabId` and `frameId` do not match the frame."));
    }
  } else if (tab_id.has_value()) {
    if (!frame_id.has_value()) {
      // Default to main frame if no frame is provided.
      frame_id = 0;
    }

    if (!ExtensionTabUtil::GetTabById(tab_id.value(), browser_context(),
                                      include_incognito_information(),
                                      &web_contents) ||
        !web_contents) {
      return RespondNow(Error("Invalid `tabId`."));
    }
    render_frame_host = ExtensionApiFrameIdMap::GetRenderFrameHostById(
        web_contents, frame_id.value());
    if (!render_frame_host) {
      return RespondNow(Error("Invalid `frameId`."));
    }
  } else if (frame_id.has_value()) {
    if (frame_id.value() == 0) {
      return RespondNow(
          Error("`frameId` may not be 0 if no `tabId` is present."));
    }
    if (frame_id.value() < 0) {
      return RespondNow(Error("Invalid `frameId`."));
    }

    render_frame_host =
        ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByFrameId(
            frame_id.value());
    if (!render_frame_host) {
      return RespondNow(Error("Invalid `frameId`."));
    }
    web_contents = content::WebContents::FromRenderFrameHost(render_frame_host);
    if (!web_contents ||
        !util::IsWebContentsInContext(*web_contents, *browser_context(),
                                      include_incognito_information())) {
      return RespondNow(Error("Invalid `frameId`."));
    }
  } else {
    return RespondNow(
        Error("Either `documentId` or `tabId` must be specified."));
  }

  CHECK(render_frame_host);

  base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                 std::string>
      serialized_key = net::CookiePartitionKey::Serialize(
          render_frame_host->GetStorageKey().ToCookiePartitionKey());
  if (!serialized_key.has_value()) {
    return RespondNow(Error("PartitionKey requested is not serializable."));
  }

  std::string error;
  if (!CheckHostPermissions(extension(), GURL(serialized_key->TopLevelSite()),
                            &error) ||
      !CheckHostPermissions(extension(),
                            render_frame_host->GetLastCommittedURL(), &error)) {
    return RespondNow(Error(error));
  }

  api::cookies::CookiePartitionKey partition_key;
  partition_key.has_cross_site_ancestor =
      serialized_key->has_cross_site_ancestor();
  partition_key.top_level_site = serialized_key->TopLevelSite();

  api::cookies::GetPartitionKey::Results::Details details =
      api::cookies::GetPartitionKey::Results::Details();
  details.partition_key = partition_key.Clone();
  return RespondNow(WithArguments(details.ToValue()));
}

ExtensionFunction::ResponseAction CookiesGetAllCookieStoresFunction::Run() {
  // Return a list of all cookie stores with at least one open tab.
  std::vector<api::cookies::CookieStore> cookie_stores;
  for (auto& store_context :
       ExtensionsAPIClient::Get()
           ->GetCookiesApiDelegate()
           ->GetCookieStoreContexts(*browser_context(),
                                    include_incognito_information())) {
    base::ListValue tab_ids;
    for (int tab_id : store_context.tab_ids) {
      tab_ids.Append(tab_id);
    }
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        &*store_context.browser_context, std::move(tab_ids)));
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
  DCHECK(!cookies_event_router_);
  cookies_event_router_ =
      std::make_unique<CookiesEventRouter>(browser_context_);
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions

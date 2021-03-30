// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service.h"

#include <stdint.h>

#include <utility>

#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service_factory.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_result.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace api_launcher_search_provider =
    extensions::api::launcher_search_provider;
using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;
using extensions::ExtensionId;
using extensions::ExtensionSet;

namespace chromeos {
namespace launcher_search_provider {

Service::Service(Profile* profile,
                 extensions::ExtensionRegistry* extension_registry)
    : profile_(profile), extension_registry_(extension_registry) {
  extension_registry_->AddObserver(this);
}

Service::~Service() {
  extension_registry_->RemoveObserver(this);
}

// static
Service* Service::Get(content::BrowserContext* context) {
  return ServiceFactory::Get(context);
}

void Service::OnQueryStarted(app_list::LauncherSearchProvider* provider,
                             const std::string& query,
                             const int max_result) {
  DCHECK(!is_query_running_);
  query_ = query;
  is_query_running_ = true;
  provider_ = provider;

  ++query_id_;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  CacheListenerExtensionIds();
  for (const ExtensionId& extension_id :
       *cached_listener_extension_ids_.get()) {
    // Convert query_id_ to string here since queryId is defined as string in
    // javascript side API while we use uint32_t internally to generate it.
    event_router->DispatchEventToExtension(
        extension_id,
        std::make_unique<extensions::Event>(
            extensions::events::LAUNCHER_SEARCH_PROVIDER_ON_QUERY_STARTED,
            api_launcher_search_provider::OnQueryStarted::kEventName,
            api_launcher_search_provider::OnQueryStarted::Create(
                query_id_, query, max_result)));
  }
}

void Service::OnQueryEnded() {
  DCHECK(is_query_running_);
  provider_ = nullptr;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  CacheListenerExtensionIds();
  for (const ExtensionId& extension_id :
       *cached_listener_extension_ids_.get()) {
    event_router->DispatchEventToExtension(
        extension_id,
        std::make_unique<extensions::Event>(
            extensions::events::LAUNCHER_SEARCH_PROVIDER_ON_QUERY_ENDED,
            api_launcher_search_provider::OnQueryEnded::kEventName,
            api_launcher_search_provider::OnQueryEnded::Create(query_id_)));
  }

  is_query_running_ = false;
}

void Service::OnOpenResult(const ExtensionId& extension_id,
                           const std::string& item_id) {
  CacheListenerExtensionIds();
  CHECK(base::Contains(*cached_listener_extension_ids_.get(), extension_id));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  event_router->DispatchEventToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::LAUNCHER_SEARCH_PROVIDER_ON_OPEN_RESULT,
          api_launcher_search_provider::OnOpenResult::kEventName,
          api_launcher_search_provider::OnOpenResult::Create(item_id)));
}

void Service::SetSearchResults(
    const extensions::Extension* extension,
    std::unique_ptr<ErrorReporter> error_reporter,
    const int query_id,
    const std::vector<extensions::api::launcher_search_provider::SearchResult>&
        results) {
  // If query is not running or query_id is different from current query id,
  // discard the results.
  if (!is_query_running_ || query_id != query_id_)
    return;

  // If |extension| is not in the listener extensions list, ignore it.
  CacheListenerExtensionIds();
  if (!base::Contains(*cached_listener_extension_ids_.get(), extension->id())) {
    return;
  }

  // Set search results to provider.
  DCHECK(provider_);
  std::vector<std::unique_ptr<app_list::LauncherSearchResult>> search_results;
  for (const auto& result : results) {
    const int relevance =
        base::ClampToRange(result.relevance, 0, kMaxSearchResultScore);

    const std::string icon_type =
        result.icon_type ? *result.icon_type.get() : std::string();

    // Calculate the relevance score by matching the query with the title.
    // Results with a match score of 0 are discarded. This will also be used to
    // set the title tags (highlighting which parts of the title matched the
    // search query).
    const std::u16string title = base::UTF8ToUTF16(result.title);
    TokenizedString tokenized_title(title);
    TokenizedStringMatch match;
    TokenizedString tokenized_query(base::UTF8ToUTF16(query_));
    if (!match.Calculate(tokenized_query, tokenized_title))
      continue;

    auto search_result = std::make_unique<app_list::LauncherSearchResult>(
        result.item_id, icon_type, relevance, profile_, extension,
        error_reporter->Duplicate());
    search_result->UpdateFromMatch(tokenized_title, match);
    search_results.push_back(std::move(search_result));
  }
  provider_->SetSearchResults(extension->id(), std::move(search_results));
}

bool Service::IsQueryRunning() const {
  return is_query_running_;
}

void Service::OnExtensionLoaded(content::BrowserContext* browser_context,
                                const extensions::Extension* extension) {
  // Invalidate cache.
  cached_listener_extension_ids_.reset();
}

void Service::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  extensions::UnloadedExtensionReason reason) {
  // Invalidate cache.
  cached_listener_extension_ids_.reset();
}

void Service::CacheListenerExtensionIds() {
  // If it's already cached, do nothing.
  if (cached_listener_extension_ids_)
    return;

  cached_listener_extension_ids_.reset(new std::set<ExtensionId>());

  const ExtensionSet& extension_set = extension_registry_->enabled_extensions();
  for (scoped_refptr<const extensions::Extension> extension : extension_set) {
    const extensions::PermissionsData* permission_data =
        extension->permissions_data();
    const bool has_permission = permission_data->HasAPIPermission(
        extensions::mojom::APIPermissionID::kLauncherSearchProvider);
    if (has_permission)
      cached_listener_extension_ids_->insert(extension->id());
  }
}

}  // namespace launcher_search_provider
}  // namespace chromeos

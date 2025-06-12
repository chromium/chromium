// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/autocomplete_dictionary_preload_service.h"

#include <vector>

#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/storage_partition.h"

AutocompleteDictionaryPreloadService::AutocompleteDictionaryPreloadService(
    Profile& profile)
    : profile_(profile) {}

AutocompleteDictionaryPreloadService::~AutocompleteDictionaryPreloadService() =
    default;

void AutocompleteDictionaryPreloadService::MaybePreload(
    const AutocompleteResult& result) {
  if (!base::FeatureList::IsEnabled(kAutocompleteDictionaryPreload)) {
    return;
  }
  std::vector<GURL> match_destination_urls;
  match_destination_urls.reserve(result.size());
  for (const AutocompleteMatch& match : result) {
    if (match.destination_url.SchemeIsHTTPOrHTTPS()) {
      match_destination_urls.emplace_back(match.destination_url);
    }
  }

  if (match_destination_urls.empty()) {
    return;
  }

  // Keep the old handle until `PreloadSharedDictionaryInfoForDocument()` call
  // to avoid reloading dictionaries in the network service.
  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      old_handle = std::move(preloaded_shared_dictionaries_handle_);

  preloaded_shared_dictionaries_handle_.reset();
  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->PreloadSharedDictionaryInfoForDocument(
          match_destination_urls, preloaded_shared_dictionaries_handle_
                                      .InitWithNewPipeAndPassReceiver());
  preloaded_shared_dictionaries_expiry_timer_.Start(
      FROM_HERE, kAutocompletePreloadedDictionaryTimeout.Get(),
      base::BindOnce(
          &AutocompleteDictionaryPreloadService::DeletePreloadedDictionaries,
          base::Unretained(this)));
}

void AutocompleteDictionaryPreloadService::DeletePreloadedDictionaries() {
  preloaded_shared_dictionaries_handle_.reset();
}

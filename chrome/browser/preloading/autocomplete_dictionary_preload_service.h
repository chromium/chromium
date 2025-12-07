// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

class AutocompleteResult;
class Profile;

// This service is used for preloading compression dictionaries in the network
// service for navigation on Omnibox autocomplete.
class AutocompleteDictionaryPreloadService : public KeyedService {
 public:
  explicit AutocompleteDictionaryPreloadService(Profile& profile);
  ~AutocompleteDictionaryPreloadService() override;

  // Not movable nor copyable.
  AutocompleteDictionaryPreloadService(
      const AutocompleteDictionaryPreloadService&&) = delete;
  AutocompleteDictionaryPreloadService& operator=(
      const AutocompleteDictionaryPreloadService&&) = delete;
  AutocompleteDictionaryPreloadService(
      const AutocompleteDictionaryPreloadService&) = delete;
  AutocompleteDictionaryPreloadService& operator=(
      const AutocompleteDictionaryPreloadService&) = delete;

  void MaybePreload(const AutocompleteResult& result);

 private:
  void DeletePreloadedDictionaries();

  const raw_ref<Profile> profile_;

  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      preloaded_shared_dictionaries_handle_;
  base::OneShotTimer preloaded_shared_dictionaries_expiry_timer_;
};

#endif  // CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_H_

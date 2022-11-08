// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_ENTITY_IMAGE_SERVICE_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_ENTITY_IMAGE_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace history_clusters {

// Used to get the image URL associated with a cluster. It doesn't actually
// fetch the image, that's up to the UI to do.
class EntityImageService : public KeyedService {
 public:
  using ResultCallback = base::OnceCallback<void(const GURL& image_url)>;

  // This should only be called by the internal factory. Most callers should use
  // the `Get()` method instead.
  explicit EntityImageService(Profile* profile);
  EntityImageService(const EntityImageService&) = delete;
  EntityImageService& operator=(const EntityImageService&) = delete;

  ~EntityImageService() override;

  // Gets the fetcher associated with `profile`. Always succeeds.
  static EntityImageService* Get(Profile* profile);

  // Fetches an image appropriate for `search_query` and `entity_id`, returning
  // the result asynchronously to `callback`. Returns false if we can't do it
  // for configuration or privacy reasons.
  bool FetchImageFor(const std::u16string& search_query,
                     const std::string& entity_id,
                     ResultCallback callback);

 private:
  class SuggestEntityImageURLFetcher;

  // Callback for `FetchImageFor`.
  void OnImageFetched(std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
                      ResultCallback callback,
                      const GURL& image_url);

  Profile* const profile_;
  ChromeAutocompleteProviderClient autocomplete_provider_client_;

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_consent_helper_;

  base::WeakPtrFactory<EntityImageService> weak_factory_{this};
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_ENTITY_IMAGE_SERVICE_H_

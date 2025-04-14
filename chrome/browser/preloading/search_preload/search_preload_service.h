// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/preloading/search_preload/search_preload_pipeline_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service_observer.h"

class AutocompleteResult;
class Profile;
class TemplateURLService;
struct AutocompleteMatch;

namespace content {
class WebContents;
}

namespace omnibox::mojom {
enum class NavigationPredictor;
}

// Roles:
//
// - Observes changes of `TemplateURLService` and notifies it to
//   `SearchPreloadPipelineManager`s.
// - Routes Omnibox events to `SearchPreloadPipelineManager`s.
//
// Note that
//
// - Prerender is managed per `WebContents` and we must trigger prerender for
//   appropriate `WebContents`; and
// - Prefetch is managed per `BrowserContext` and it's (theoretically) available
//   even we trigger prefetches over different `WebContents`s.
//   - Note that current behavior of `PrefetchHandle` is
//     `PrefetchHandle::dtor()` immediately destroys `PrefetchContainer` and
//     it's actually not available.
//
// So, we manage pipelines in `SearchPreloadPipelineManager` per `WebContents`.
// It's for the necessity of prerender and the simplicity of prefetch.
class SearchPreloadService : public KeyedService,
                             public TemplateURLServiceObserver {
 public:
  static SearchPreloadService* GetForProfile(Profile* profile);

  explicit SearchPreloadService(Profile* profile);
  ~SearchPreloadService() override;

  // Not movable nor copyable.
  SearchPreloadService(const SearchPreloadService&&) = delete;
  SearchPreloadService& operator=(const SearchPreloadService&&) = delete;
  SearchPreloadService(const SearchPreloadService&) = delete;
  SearchPreloadService& operator=(const SearchPreloadService&) = delete;

  base::WeakPtr<SearchPreloadService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // KeyedService:
  void Shutdown() override;

  // TemplateURLServiceObserver:
  //
  // Monitors changes to DSE. If a change occurs, clears preloads.
  void OnTemplateURLServiceChanged() override;

  // Clears all preloads from the service.
  void ClearPreloads();

  // Called when autocomplete is updated.
  void OnAutocompleteResultChanged(content::WebContents* web_contents,
                                   const AutocompleteResult& result);

  // Called when a user is likely navigate to the match.
  bool OnNavigationLikely(
      size_t index,
      const AutocompleteMatch& match,
      omnibox::mojom::NavigationPredictor navigation_predictor,
      content::WebContents* web_contents);

 private:
  // Reference is valid only as rvalue.
  SearchPreloadPipelineManager& GetOrCreatePipelineManagerWithLimit(
      content::WebContents& web_contents);

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observer_{this};

  const raw_ptr<Profile> profile_;

  std::optional<base::WeakPtr<SearchPreloadPipelineManager>> pipeline_manager_;

  base::WeakPtrFactory<SearchPreloadService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_SERVICE_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/search_preload/search_preload_pipeline.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

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
// - Manages preload pipelines per WebContents.
// - Starts preloads in response to Omnibox events.
class SearchPreloadPipelineManager
    : public content::WebContentsUserData<SearchPreloadPipelineManager> {
 public:
  ~SearchPreloadPipelineManager() override;

  // Not movable nor copyable.
  SearchPreloadPipelineManager(const SearchPreloadPipelineManager&&) = delete;
  SearchPreloadPipelineManager& operator=(
      const SearchPreloadPipelineManager&&) = delete;
  SearchPreloadPipelineManager(const SearchPreloadPipelineManager&) = delete;
  SearchPreloadPipelineManager& operator=(const SearchPreloadPipelineManager&) =
      delete;

  base::WeakPtr<SearchPreloadPipelineManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Clears all preloads.
  void ClearPreloads();

  // Called when autocomplete is updated.
  void OnAutocompleteResultChanged(Profile& profile,
                                   const AutocompleteResult& result);

  // Called when a user is likely to navigate to the match.
  //
  // Returns true iff a new prefetch is triggered by this call. Note that it
  // returns false if a prefetch for the same canonical URL has already
  // triggered.
  bool OnNavigationLikely(
      Profile& profile,
      const AutocompleteMatch& match,
      omnibox::mojom::NavigationPredictor navigation_predictor);

 private:
  friend content::WebContentsUserData<SearchPreloadPipelineManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  explicit SearchPreloadPipelineManager(content::WebContents* contents);

  void OnAutocompleteResultChangedProcessOne(
      Profile& profile,
      TemplateURLService& template_url_service,
      const AutocompleteMatch& match);

  // Manages pipeline per canonical URL.
  base::flat_map<GURL, std::unique_ptr<SearchPreloadPipeline>> pipelines_;

  base::WeakPtrFactory<SearchPreloadPipelineManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_MANAGER_H_

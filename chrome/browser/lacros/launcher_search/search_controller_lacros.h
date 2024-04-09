// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Implements crosapi interface for launcher search controller.
class SearchControllerLacros : public mojom::SearchController,
                               public AutocompleteController::Observer,
                               public ProfileObserver {
 public:
  // Does not automatically register with Ash's `SearchControllerRegistry`.
  // Call `RegisterWithAsh()` to do so.
  // `provider_types` is a bitmap containing `AutocompleteProvider::Type` values
  // to control which types of search results are returned from the
  // `AutocompleteController`.
  explicit SearchControllerLacros(int provider_types);
  SearchControllerLacros(const SearchControllerLacros&) = delete;
  SearchControllerLacros& operator=(const SearchControllerLacros&) = delete;
  ~SearchControllerLacros() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Registers this with Ash's `SearchControllerRegistry`.
  void RegisterWithAsh();

 private:
  // mojom::SearchController:
  void Search(const std::u16string& query, SearchCallback callback) override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  std::unique_ptr<FaviconCache> favicon_cache_;

  std::u16string query_;
  AutocompleteInput input_;

  mojo::AssociatedRemote<mojom::SearchResultsPublisher> publisher_;
  mojo::Receiver<mojom::SearchController> receiver_{this};

  // Observes the profile destruction.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<SearchControllerLacros> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_

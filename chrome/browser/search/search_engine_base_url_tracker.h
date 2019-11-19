// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_ENGINE_BASE_URL_TRACKER_H_
#define CHROME_BROWSER_SEARCH_SEARCH_ENGINE_BASE_URL_TRACKER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"

class SearchTermsData;

// A helper class that watches for changes to the base URL of the default search
// engine. Typically this changes when a different DSE is selected. For Google,
// it can also change without changing the DSE, when the Google base URL is
// updated. This can happen in the case of country (i.e. TLD) changes.
class SearchEngineBaseURLTracker : public TemplateURLServiceObserver {
 public:
  enum class ChangeReason {
    DEFAULT_SEARCH_PROVIDER,
    GOOGLE_BASE_URL,
  };

  using BaseURLChangedCallback = base::Callback<void(ChangeReason)>;

  SearchEngineBaseURLTracker(
      TemplateURLService* template_url_service,
      std::unique_ptr<SearchTermsData> search_terms_data,
      const BaseURLChangedCallback& base_url_changed_callback);
  ~SearchEngineBaseURLTracker() override;

 private:
  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;

  // Returns true if the base URL of the current search engine is Google.
  bool HasGoogleBaseURL();

  TemplateURLService* template_url_service_;
  std::unique_ptr<SearchTermsData> search_terms_data_;
  BaseURLChangedCallback base_url_changed_callback_;

  ScopedObserver<TemplateURLService, TemplateURLServiceObserver> observer_{
      this};

  // Used to check whether notifications from TemplateURLService indicate a
  // change that affects the default search provider.
  GURL previous_google_base_url_;
  base::Optional<TemplateURLData> previous_default_search_provider_data_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineBaseURLTracker);
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_ENGINE_BASE_URL_TRACKER_H_

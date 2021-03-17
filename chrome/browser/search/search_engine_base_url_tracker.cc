// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_engine_base_url_tracker.h"


#include "components/search_engines/search_terms_data.h"

SearchEngineBaseURLTracker::SearchEngineBaseURLTracker(
    TemplateURLService* template_url_service,
    std::unique_ptr<SearchTermsData> search_terms_data,
    const BaseURLChangedCallback& base_url_changed_callback)
    : template_url_service_(template_url_service),
      search_terms_data_(std::move(search_terms_data)),
      base_url_changed_callback_(base_url_changed_callback),
      previous_google_base_url_(search_terms_data_->GoogleBaseURLValue()) {
  DCHECK(template_url_service_);

  observer_.Add(template_url_service_);

  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (default_search_provider)
    previous_default_search_provider_data_ = default_search_provider->data();
}

SearchEngineBaseURLTracker::~SearchEngineBaseURLTracker() = default;

void SearchEngineBaseURLTracker::OnTemplateURLServiceChanged() {
  GURL google_base_url;
  if (HasGoogleBaseURL())
    google_base_url = GURL(search_terms_data_->GoogleBaseURLValue());

  // Check whether the default search provider was changed.
  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  const TemplateURLData* previous_data =
      previous_default_search_provider_data_.has_value()
          ? &previous_default_search_provider_data_.value()
          : nullptr;
  bool default_search_provider_changed = !TemplateURL::MatchesData(
      template_url, previous_data, *search_terms_data_);
  if (default_search_provider_changed) {
    if (template_url)
      previous_default_search_provider_data_ = template_url->data();
    else
      previous_default_search_provider_data_ = base::nullopt;

    // Also update the cached Google base URL, without separately notifying.
    previous_google_base_url_ = google_base_url;

    base_url_changed_callback_.Run(ChangeReason::DEFAULT_SEARCH_PROVIDER);
    return;
  }

  // Check whether the Google base URL has changed.
  // Note that, even if the TemplateURL for the Default Search Provider has not
  // changed, the effective URLs might change if they reference the Google base
  // URL. The TemplateURLService will notify us when the effective URL changes
  // in this way but it's up to us to do the work to check both.
  if (google_base_url != previous_google_base_url_) {
    previous_google_base_url_ = google_base_url;
    if (HasGoogleBaseURL())
      base_url_changed_callback_.Run(ChangeReason::GOOGLE_BASE_URL);
  }
}

bool SearchEngineBaseURLTracker::HasGoogleBaseURL() {
  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();

  return template_url && template_url->HasGoogleBaseURLs(*search_terms_data_);
}

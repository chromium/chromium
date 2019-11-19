// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_PROVIDER_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_SEARCH_PROVIDER_OBSERVER_H_

#include "chrome/browser/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

// Keeps track of any changes in search engine provider and call
// the provided callback if a third-party search provider (i.e. a third-party
// NTP) is being used.
class SearchProviderObserver : public TemplateURLServiceObserver {
 public:
  explicit SearchProviderObserver(TemplateURLService* service,
                                  base::RepeatingClosure callback);

  ~SearchProviderObserver() override;

  bool is_google() { return is_google_; }

 private:
  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  TemplateURLService* service_;
  bool is_google_;
  base::RepeatingClosure callback_;
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_PROVIDER_OBSERVER_H_

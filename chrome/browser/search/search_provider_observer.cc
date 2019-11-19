// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_provider_observer.h"

SearchProviderObserver::SearchProviderObserver(TemplateURLService* service,
                                               base::RepeatingClosure callback)
    : service_(service),
      is_google_(search::DefaultSearchProviderIsGoogle(service_)),
      callback_(std::move(callback)) {
  DCHECK(service_);
  service_->AddObserver(this);
}

SearchProviderObserver::~SearchProviderObserver() {
  if (service_)
    service_->RemoveObserver(this);
}

void SearchProviderObserver::OnTemplateURLServiceChanged() {
  is_google_ = search::DefaultSearchProviderIsGoogle(service_);
  callback_.Run();
}

void SearchProviderObserver::OnTemplateURLServiceShuttingDown() {
  service_->RemoveObserver(this);
  service_ = nullptr;
}

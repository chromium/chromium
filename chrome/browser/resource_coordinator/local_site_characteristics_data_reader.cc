// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

namespace resource_coordinator {

LocalSiteCharacteristicsDataReader::LocalSiteCharacteristicsDataReader(
    scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl)
    : impl_(std::move(impl)) {}

LocalSiteCharacteristicsDataReader::~LocalSiteCharacteristicsDataReader() {}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataReader::UpdatesFaviconInBackground() const {
  return impl_->UpdatesFaviconInBackground();
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataReader::UpdatesTitleInBackground() const {
  return impl_->UpdatesTitleInBackground();
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataReader::UsesAudioInBackground() const {
  return impl_->UsesAudioInBackground();
}

performance_manager::SiteFeatureUsage
LocalSiteCharacteristicsDataReader::UsesNotificationsInBackground() const {
  return impl_->UsesNotificationsInBackground();
}

bool LocalSiteCharacteristicsDataReader::DataLoaded() const {
  return impl_->DataLoaded();
}

void LocalSiteCharacteristicsDataReader::RegisterDataLoadedCallback(
    base::OnceClosure&& callback) {
  // Register a closure that is bound using a weak pointer to this instance.
  // In that way it won't be invoked by the underlying |impl_| after this
  // reader is destroyed.
  base::OnceClosure closure(
      base::BindOnce(&LocalSiteCharacteristicsDataReader::RunClosure,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  impl_->RegisterDataLoadedCallback(std::move(closure));
}

void LocalSiteCharacteristicsDataReader::RunClosure(
    base::OnceClosure&& closure) {
  std::move(closure).Run();
}

}  // namespace resource_coordinator

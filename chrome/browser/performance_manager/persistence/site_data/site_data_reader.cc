// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

SiteDataReader::SiteDataReader(scoped_refptr<internal::SiteDataImpl> impl)
    : impl_(std::move(impl)) {}

SiteDataReader::~SiteDataReader() {}

performance_manager::SiteFeatureUsage
SiteDataReader::UpdatesFaviconInBackground() const {
  return impl_->UpdatesFaviconInBackground();
}

performance_manager::SiteFeatureUsage SiteDataReader::UpdatesTitleInBackground()
    const {
  return impl_->UpdatesTitleInBackground();
}

performance_manager::SiteFeatureUsage SiteDataReader::UsesAudioInBackground()
    const {
  return impl_->UsesAudioInBackground();
}

performance_manager::SiteFeatureUsage
SiteDataReader::UsesNotificationsInBackground() const {
  return impl_->UsesNotificationsInBackground();
}

bool SiteDataReader::DataLoaded() const {
  return impl_->DataLoaded();
}

void SiteDataReader::RegisterDataLoadedCallback(base::OnceClosure&& callback) {
  // Register a closure that is bound using a weak pointer to this instance.
  // In that way it won't be invoked by the underlying |impl_| after this
  // reader is destroyed.
  base::OnceClosure closure(base::BindOnce(&SiteDataReader::RunClosure,
                                           weak_factory_.GetWeakPtr(),
                                           std::move(callback)));
  impl_->RegisterDataLoadedCallback(std::move(closure));
}

void SiteDataReader::RunClosure(base::OnceClosure&& closure) {
  std::move(closure).Run();
}

}  // namespace performance_manager

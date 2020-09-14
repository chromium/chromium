// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"

#include "chrome/browser/subresource_filter/ads_intervention_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"

SubresourceFilterProfileContext::SubresourceFilterProfileContext(
    Profile* profile)
    : settings_manager_(
          std::make_unique<SubresourceFilterContentSettingsManager>(profile)),
      ads_intervention_manager_(
          std::make_unique<AdsInterventionManager>(settings_manager_.get())) {}

SubresourceFilterProfileContext::~SubresourceFilterProfileContext() {}

void SubresourceFilterProfileContext::Shutdown() {
  settings_manager_.reset();
  ads_intervention_manager_.reset();
}

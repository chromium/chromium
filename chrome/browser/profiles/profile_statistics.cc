// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_statistics_aggregator.h"

ProfileStatistics::ProfileStatistics(Profile* profile)
    : profile_(profile), aggregator_(nullptr) {}

ProfileStatistics::~ProfileStatistics() {
}

void ProfileStatistics::GatherStatistics(
    const profiles::ProfileStatisticsCallback& callback) {
  // IsValidProfile() can be false in unit tests.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_))
    return;
  DCHECK(!profile_->IsOffTheRecord() && !profile_->IsSystemProfile());

  if (!aggregator_) {
    aggregator_ = std::make_unique<ProfileStatisticsAggregator>(
        profile_, base::Bind(&ProfileStatistics::DeregisterAggregator,
                             weak_ptr_factory_.GetWeakPtr()));
  }
  aggregator_->AddCallbackAndStartAggregator(callback);
}

void ProfileStatistics::DeregisterAggregator() {
  aggregator_ = nullptr;
}


// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_data_counter.h"

#include "base/bind.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
bool CheckSyncState(Profile* profile, const syncer::SyncService* sync_service) {
  return browsing_data_counter_utils::ShouldShowCookieException(profile);
}
}  // namespace

SiteDataCounter::SiteDataCounter(Profile* profile)
    : profile_(profile),
      sync_tracker_(this, ProfileSyncServiceFactory::GetForProfile(profile)) {}

SiteDataCounter::~SiteDataCounter() {}

void SiteDataCounter::OnInitialized() {
  sync_tracker_.OnInitialized(
      base::BindRepeating(&CheckSyncState, base::Unretained(profile_)));
}

const char* SiteDataCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteCookies;
}

void SiteDataCounter::Count() {
  // Cancel existing requests.
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::Time begin = GetPeriodStart();
  auto done_callback =
      base::BindOnce(&SiteDataCounter::Done, weak_ptr_factory_.GetWeakPtr());
  // Use a helper class that owns itself to avoid issues when SiteDataCounter is
  // deleted before counting finished.
  auto* helper =
      new SiteDataCountingHelper(profile_, begin, std::move(done_callback));
  helper->CountAndDestroySelfWhenFinished();
}

void SiteDataCounter::Done(int origin_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ReportResult(origin_count);
}

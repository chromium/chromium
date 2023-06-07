// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

constexpr base::TimeDelta kTimerPeriod = base::Days(1);

StorageAccessAPIServiceImpl::StorageAccessAPIServiceImpl(
    content::BrowserContext* browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(browser_context);

  base::Time now = base::Time::Now();
  // We do our best to update the profile's state starting at the next midnight.
  base::Time next_midnight = now.LocalMidnight() + base::Days(1);
  base::TimeDelta to_next_midnight = next_midnight - now;

  // Daylight savings means that some days are longer than 24 hours and some are
  // shorter than 24 hours, but the next midnight should never be more than 2
  // days away.
  CHECK_LT(to_next_midnight, base::Days(2)) << now;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StorageAccessAPIServiceImpl::StartPeriodicTimer,
                     weak_ptr_factory_.GetWeakPtr()),
      to_next_midnight);
}

StorageAccessAPIServiceImpl::~StorageAccessAPIServiceImpl() = default;

bool StorageAccessAPIServiceImpl::RenewPermissionGrant(
    const url::Origin& embedded_origin,
    const url::Origin& top_frame_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!updated_grants_.Insert(embedded_origin, top_frame_origin)) {
    return false;
  }

  // TODO(https://crbug.com/1450356): implement grant renewal.
  return true;
}

void StorageAccessAPIServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void StorageAccessAPIServiceImpl::StartPeriodicTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPeriodicTimerFired();
  periodic_timer_.Start(
      FROM_HERE, kTimerPeriod,
      base::BindRepeating(&StorageAccessAPIServiceImpl::OnPeriodicTimerFired,
                          weak_ptr_factory_.GetWeakPtr()));
}

void StorageAccessAPIServiceImpl::OnPeriodicTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  updated_grants_.Clear();
}

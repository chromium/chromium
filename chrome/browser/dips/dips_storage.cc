// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/dips_utils.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

DIPSStorage::DIPSStorage(const std::optional<base::FilePath>& path)
    : db_(std::make_unique<DIPSDatabase>(path)) {
  base::AssertLongCPUWorkAllowed();
}

DIPSStorage::~DIPSStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// DIPSDatabase interaction functions ------------------------------------------

DIPSState DIPSStorage::Read(const GURL& url) {
  return ReadSite(GetSiteForDIPS(url));
}

DIPSState DIPSStorage::ReadSite(std::string site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  std::optional<StateValue> state = db_->Read(site);

  if (state.has_value()) {
    // We should not have entries in the DB without any timestamps.
    DCHECK(state->site_storage_times.has_value() ||
           state->user_interaction_times.has_value() ||
           state->stateful_bounce_times.has_value() ||
           state->bounce_times.has_value() ||
           state->web_authn_assertion_times.has_value());

    return DIPSState(this, std::move(site), state.value());
  }
  return DIPSState(this, std::move(site));
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  db_->Write(state.site(), state.site_storage_times(),
             state.user_interaction_times(), state.stateful_bounce_times(),
             state.bounce_times(), state.web_authn_assertion_times());
}

std::optional<PopupsStateValue> DIPSStorage::ReadPopup(
    const std::string& first_party_site,
    const std::string& tracking_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  return db_->ReadPopup(first_party_site, tracking_site);
}

std::vector<PopupWithTime> DIPSStorage::ReadRecentPopupsWithInteraction(
    const base::TimeDelta& lookback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  return db_->ReadRecentPopupsWithInteraction(lookback);
}

bool DIPSStorage::WritePopup(const std::string& first_party_site,
                             const std::string& tracking_site,
                             const uint64_t access_id,
                             const base::Time& popup_time,
                             bool is_current_interaction,
                             bool is_authentication_interaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  return db_->WritePopup(first_party_site, tracking_site, access_id, popup_time,
                         is_current_interaction, is_authentication_interaction);
}

void DIPSStorage::RemoveEvents(base::Time delete_begin,
                               base::Time delete_end,
                               network::mojom::ClearDataFilterPtr filter,
                               const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DCHECK(delete_end.is_null() || delete_begin <= delete_end);

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (filter.is_null()) {
    db_->RemoveEventsByTime(delete_begin, delete_end, type);
  } else if (type == DIPSEventRemovalType::kStorage &&
             filter->origins.empty()) {
    // Site-filtered deletion is only supported for cookie-related
    // DIPS events, since only cookie deletion allows domains but not hosts.
    //
    // TODO(jdh): Assess the use of cookie deletions with both a time range and
    // a list of domains to determine whether supporting time ranges here is
    // necessary.
    // Time ranges aren't currently supported for site-filtered
    // deletion of DIPS Events.
    if (delete_begin != base::Time::Min() || delete_end != base::Time::Max()) {
      // TODO (kaklilu@): Add a UMA metric to record if this happens.
      return;
    }

    bool preserve =
        (filter->type == network::mojom::ClearDataFilter::Type::KEEP_MATCHES);
    std::vector<std::string> sites = std::move(filter->domains);

    db_->RemoveEventsBySite(preserve, sites, type);
  }
}

void DIPSStorage::RemoveRows(const std::vector<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  db_->RemoveRows(DIPSDatabaseTable::kBounces, sites);
}

void DIPSStorage::RemoveRowsWithoutProtectiveEvent(
    const std::set<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  std::set<std::string> filtered_sites =
      FilterSitesWithoutProtectiveEvent(sites);

  RemoveRows(
      std::vector<std::string>(filtered_sites.begin(), filtered_sites.end()));
}

// DIPSTabHelper Function Impls ------------------------------------------------

void DIPSStorage::RecordStorage(const GURL& url,
                                base::Time time,
                                DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  state.update_site_storage_time(time);
}

void DIPSStorage::RecordInteraction(const GURL& url,
                                    base::Time time,
                                    DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  state.update_user_interaction_time(time);
}

void DIPSStorage::RecordWebAuthnAssertion(const GURL& url,
                                          base::Time time,
                                          DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  state.update_web_authn_assertion_time(time);
}

void DIPSStorage::RecordBounce(const GURL& url,
                               base::Time time,
                               bool stateful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DIPSState state = Read(url);
  state.update_bounce_time(time);
  if (stateful) {
    state.update_stateful_bounce_time(time);
  }
}

std::set<std::string> DIPSStorage::FilterSitesWithoutProtectiveEvent(
    std::set<std::string> sites) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  std::set<std::string> interacted_sites =
      db_->FilterSitesWithProtectiveEvent(sites);

  for (const auto& site : interacted_sites) {
    if (sites.count(site)) {
      sites.erase(site);
    }
  }

  return sites;
}

std::vector<std::string> DIPSStorage::GetSitesThatBounced(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBounced(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesThatBouncedWithState(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBouncedWithState(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesThatUsedStorage(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatUsedStorage(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesToClear(
    std::optional<base::TimeDelta> custom_period) const {
  std::vector<std::string> sites_to_clear;
  base::TimeDelta grace_period =
      custom_period.value_or(features::kDIPSGracePeriod.Get());

  switch (features::kDIPSTriggeringAction.Get()) {
    case content::DIPSTriggeringAction::kNone: {
      return {};
    }
    case content::DIPSTriggeringAction::kStorage: {
      sites_to_clear = GetSitesThatUsedStorage(grace_period);
      break;
    }
    case content::DIPSTriggeringAction::kBounce: {
      sites_to_clear = GetSitesThatBounced(grace_period);
      break;
    }
    case content::DIPSTriggeringAction::kStatefulBounce: {
      sites_to_clear = GetSitesThatBouncedWithState(grace_period);
      break;
    }
  }

  return sites_to_clear;
}

bool DIPSStorage::DidSiteHaveInteractionSince(const GURL& url,
                                              base::Time bound) {
  auto last_user_interaction_time = LastInteractionTime(url);
  return last_user_interaction_time.has_value() &&
         last_user_interaction_time >= bound;
}

std::optional<base::Time> DIPSStorage::LastInteractionTime(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const DIPSState state = Read(url);
  if (!state.user_interaction_times().has_value()) {
    return std::nullopt;
  }
  return state.user_interaction_times()->second;
}

/* static */
void DIPSStorage::DeleteDatabaseFiles(base::FilePath path,
                                      base::OnceClosure on_complete) {
  // TODO (jdh): Decide how to handle the case of failing to delete db files.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(IgnoreResult(&sql::Database::Delete), std::move(path)),
      std::move(on_complete));
}

std::optional<base::Time> DIPSStorage::GetTimerLastFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetTimerLastFired();
}

bool DIPSStorage::SetTimerLastFired(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->SetTimerLastFired(time);
}

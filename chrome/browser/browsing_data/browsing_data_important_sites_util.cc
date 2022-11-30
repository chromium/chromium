// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"

#include "base/scoped_observation.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "content/public/browser/browsing_data_filter_builder.h"

namespace {

// This object receives |task_count| calls from BrowsingDataRemover, calls
// |callback| when all tasks are finished and destroys itself.
class BrowsingDataTaskObserver : public content::BrowsingDataRemover::Observer {
 public:
  BrowsingDataTaskObserver(content::BrowsingDataRemover* remover,
                           base::OnceCallback<void(uint64_t)> callback,
                           int task_count);

  BrowsingDataTaskObserver(const BrowsingDataTaskObserver&) = delete;
  BrowsingDataTaskObserver& operator=(const BrowsingDataTaskObserver&) = delete;

  ~BrowsingDataTaskObserver() override;

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

 private:
  base::OnceCallback<void(uint64_t)> callback_;
  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      remover_observation_{this};
  int task_count_;
  uint64_t failed_data_types_ = 0;
};

BrowsingDataTaskObserver::BrowsingDataTaskObserver(
    content::BrowsingDataRemover* remover,
    base::OnceCallback<void(uint64_t)> callback,
    int task_count)
    : callback_(std::move(callback)),
      task_count_(task_count) {
  remover_observation_.Observe(remover);
}

BrowsingDataTaskObserver::~BrowsingDataTaskObserver() = default;

void BrowsingDataTaskObserver::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  DCHECK(task_count_);
  failed_data_types_ |= failed_data_types;
  if (--task_count_)
    return;
  remover_observation_.Reset();
  std::move(callback_).Run(failed_data_types_);
  delete this;
}

}  // namespace

namespace browsing_data_important_sites_util {

void Remove(uint64_t remove_mask,
            uint64_t origin_mask,
            browsing_data::TimePeriod time_period,
            std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
            content::BrowsingDataRemover* remover,
            base::OnceCallback<void(uint64_t)> callback) {
  auto* observer =
      new BrowsingDataTaskObserver(remover, std::move(callback), 2);

  uint64_t filterable_mask = 0;
  uint64_t nonfilterable_mask = remove_mask;

  if (!filter_builder->MatchesAllOriginsAndDomains()) {
    filterable_mask =
        remove_mask & chrome_browsing_data_remover::IMPORTANT_SITES_DATA_TYPES;
    nonfilterable_mask =
        remove_mask & ~chrome_browsing_data_remover::IMPORTANT_SITES_DATA_TYPES;
  }
  browsing_data::RecordDeletionForPeriod(time_period);

  if (nonfilterable_mask) {
    remover->RemoveAndReply(
        browsing_data::CalculateBeginDeleteTime(time_period),
        browsing_data::CalculateEndDeleteTime(time_period), nonfilterable_mask,
        origin_mask, observer);
  } else {
    observer->OnBrowsingDataRemoverDone(/*failed_data_types=*/0);
  }

  // Cookie deletion could be deferred until all other data types are deleted.
  // As cookie deletion may be filtered, this needs to happen last.
  if (filterable_mask) {
    remover->RemoveWithFilterAndReply(
        browsing_data::CalculateBeginDeleteTime(time_period),
        browsing_data::CalculateEndDeleteTime(time_period), filterable_mask,
        origin_mask, std::move(filter_builder), observer);
  } else {
    observer->OnBrowsingDataRemoverDone(/*failed_data_types=*/0);
  }
}

}  // namespace browsing_data_important_sites_util

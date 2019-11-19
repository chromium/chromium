// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Name prefix of the histogram that counts the number of reports uploaded by a
// metric provider.
const char kUploadCountHistogramPrefix[] = "ChromeOS.CWP.Upload";

// An upper bound on the count of reports expected to be uploaded by an UMA
// callback.
const int kMaxValueUploadReports = 10;

}  // namespace

using MetricCollector = internal::MetricCollector;

MetricProvider::MetricProvider(std::unique_ptr<MetricCollector> collector)
    : upload_uma_histogram_(std::string(kUploadCountHistogramPrefix) +
                            collector->ToolName()),
      // Run the collector at a higher priority to enable fast triggering of
      // profile collections. In particular, we want fast triggering when
      // jankiness is detected, but even random based periodic collection
      // benefits from a higher priority, to avoid biasing the collection to
      // times when the system is not busy. The work performed on the dedicated
      // sequence is short and infrequent. Expensive parsing operations are
      // executed asynchronously on the thread pool.
      collector_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE})),
      metric_collector_(std::move(collector)),
      weak_factory_(this) {
  metric_collector_->set_profile_done_callback(base::BindRepeating(
      &MetricProvider::OnProfileDone, weak_factory_.GetWeakPtr()));
}

MetricProvider::~MetricProvider() {
  // Destroy the metric_collector_ on the collector sequence.
  collector_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<MetricCollector> collector_) {},
                     std::move(metric_collector_)));
}

void MetricProvider::Init() {
  // It is safe to use base::Unretained to post tasks to the metric_collector_
  // on the collector sequence, since we control its lifetime. Any tasks
  // posted to it are bound to run before we destroy it on the collector
  // sequence.
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::Init,
                                base::Unretained(metric_collector_.get())));
}

bool MetricProvider::GetSampledProfiles(
    std::vector<SampledProfile>* sampled_profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (cached_profile_data_.empty()) {
    base::UmaHistogramExactLinear(upload_uma_histogram_, 0,
                                  kMaxValueUploadReports);
    return false;
  }

  base::UmaHistogramExactLinear(upload_uma_histogram_,
                                cached_profile_data_.size(),
                                kMaxValueUploadReports);
  sampled_profiles->insert(
      sampled_profiles->end(),
      std::make_move_iterator(cached_profile_data_.begin()),
      std::make_move_iterator(cached_profile_data_.end()));
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::ResetCachedDataSize,
                                base::Unretained(metric_collector_.get())));
  cached_profile_data_.clear();
  return true;
}

void MetricProvider::OnUserLoggedIn() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::TimeTicks now = base::TimeTicks::Now();
  collector_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricCollector::RecordUserLogin,
                     base::Unretained(metric_collector_.get()), now));
}

void MetricProvider::Deactivate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Notifies the collector to turn off the timer. Does not delete any data that
  // was already collected and stored in |cached_profile_data|.
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::StopTimer,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::SuspendDone(base::TimeDelta sleep_duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::ScheduleSuspendDoneCollection,
                                base::Unretained(metric_collector_.get()),
                                sleep_duration));
}

void MetricProvider::OnSessionRestoreDone(int num_tabs_restored) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collector_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricCollector::ScheduleSessionRestoreCollection,
                     base::Unretained(metric_collector_.get()),
                     num_tabs_restored));
}

// static
void MetricProvider::OnProfileDone(
    base::WeakPtr<MetricProvider> provider,
    std::unique_ptr<SampledProfile> sampled_profile) {
  base::PostTask(FROM_HERE, base::TaskTraits(content::BrowserThread::UI),
                 base::BindOnce(&MetricProvider::AddProfileToCache, provider,
                                std::move(sampled_profile)));
}

void MetricProvider::OnJankStarted() {
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::OnJankStarted,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::OnJankStopped() {
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::OnJankStopped,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::AddProfileToCache(
    std::unique_ptr<SampledProfile> sampled_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::AddCachedDataDelta,
                                base::Unretained(metric_collector_.get()),
                                sampled_profile->ByteSize()));
  cached_profile_data_.resize(cached_profile_data_.size() + 1);
  cached_profile_data_.back().Swap(sampled_profile.get());

  if (!cache_updated_callback_.is_null())
    cache_updated_callback_.Run();
}

}  // namespace metrics

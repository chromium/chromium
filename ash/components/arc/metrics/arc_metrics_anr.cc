// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_metrics_anr.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {
// Intervals to collect ANR rate after login. Collected only once per session.
// In case the session is shorter than |kMinStartPeriodDuration| interval, it
// is discarded. Shorter sessions distort stats due to ANR is long process that
// may take 10, 20 and more seconds. This also done to avoid false reporting
// when session gets restarted due to applying configuration, policy, and so on.
constexpr base::TimeDelta kMaxStartPeriodDuration = base::Minutes(10);
constexpr base::TimeDelta kMinStartPeriodDuration = base::Minutes(2);

// Interval to report ANR rate. ANR rate is reported each interval of ARC
// active state.
constexpr base::TimeDelta kRateInterval = base::Hours(4);

// Interval to update ANR interval. Once accumulated interval is
// greater than  |kArcAnrRateInterval| UMA is updated.
constexpr base::TimeDelta kUpdateInterval = base::Minutes(5);

// It is very unlikely to do have more ANR events than
// |kForPeriodMaxCount| times in |kMaxStartPeriodDuration| or
// |kRateInterval|. This acts as an upper bound of UMA buckets.
constexpr int kForPeriodMaxCount = 64;

constexpr char kStartPeriodHistogram[] = "Arc.Anr.First10MinutesAfterStart";
constexpr char kRegularPeriodHistogram[] = "Arc.Anr.Per4Hours";

// App types to report.
constexpr char kAppTypeArcAppLauncher[] = "ArcAppLauncher";
constexpr char kAppTypeArcOther[] = "ArcOther";
constexpr char kAppTypeFirstParty[] = "FirstParty";
constexpr char kAppTypeGmsCore[] = "GmsCore";
constexpr char kAppTypePlayStore[] = "PlayStore";
constexpr char kAppTypeSystemServer[] = "SystemServer";
constexpr char kAppTypeSystem[] = "SystemApp";
constexpr char kAppTypeOther[] = "Other";

std::string SourceToTableName(mojom::AnrSource value) {
  switch (value) {
    case mojom::AnrSource::OTHER:
      return kAppTypeOther;
    case mojom::AnrSource::SYSTEM_SERVER:
      return kAppTypeSystemServer;
    case mojom::AnrSource::SYSTEM_APP:
      return kAppTypeSystem;
    case mojom::AnrSource::GMS_CORE:
      return kAppTypeGmsCore;
    case mojom::AnrSource::PLAY_STORE:
      return kAppTypePlayStore;
    case mojom::AnrSource::FIRST_PARTY:
      return kAppTypeFirstParty;
    case mojom::AnrSource::ARC_OTHER:
      return kAppTypeArcOther;
    case mojom::AnrSource::ARC_APP_LAUNCHER:
      return kAppTypeArcAppLauncher;
    default:
      LOG(ERROR) << "Unrecognized source ANR " << value;
      return kAppTypeOther;
  }
}

void RecordUmaWithSuffix(const std::string& name,
                         int count,
                         int max,
                         const std::string& uma_suffix) {
  base::UmaHistogramExactLinear(name, count, max);
  if (uma_suffix.empty()) {
    LOG(ERROR) << "Boot type is unknown. Skip recording " << name
               << " with a suffix";
    return;
  }
  // In addition to e.g. Arc.Anr.Per4Hours, record e.g.
  // Arc.Anr.Per4Hours.FirstBootAfterUpdate.
  base::UmaHistogramExactLinear(
      base::StringPrintf("%s%s", name.c_str(), uma_suffix.c_str()), count, max);
}

}  // namespace

ArcMetricsAnr::ArcMetricsAnr(PrefService* prefs) : prefs_(prefs) {
  pending_start_timer_.Start(
      FROM_HERE, kMinStartPeriodDuration,
      base::BindOnce(&ArcMetricsAnr::SetLogOnStartPending,
                     base::Unretained(this)));
  start_timer_.Start(
      FROM_HERE, kMaxStartPeriodDuration,
      base::BindOnce(&ArcMetricsAnr::LogOnStart, base::Unretained(this)));
  period_updater_.Start(
      FROM_HERE, kUpdateInterval,
      base::BindRepeating(&ArcMetricsAnr::UpdateRate, base::Unretained(this)));
}

ArcMetricsAnr::~ArcMetricsAnr() {
  if (log_on_start_pending_) {
    // Session is shorter than |kMaxStartPeriodDuration| but longer than
    // |kMinStartPeriodDuration|.
    RecordUmaWithSuffix(kStartPeriodHistogram, count_10min_after_start_,
                        kForPeriodMaxCount, uma_suffix_);
  }
}

void ArcMetricsAnr::Report(mojom::AnrPtr anr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO (b/227337741): Retire these in favor of ANR rate.
  base::UmaHistogramEnumeration("Arc.Anr.Overall", anr->type);
  base::UmaHistogramEnumeration("Arc.Anr." + SourceToTableName(anr->source),
                                anr->type);
  ++count_10min_after_start_;
  prefs_->SetInteger(prefs::kAnrPendingCount,
                     prefs_->GetInteger(prefs::kAnrPendingCount) + 1);
}

void ArcMetricsAnr::LogOnStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordUmaWithSuffix(kStartPeriodHistogram, count_10min_after_start_,
                      kForPeriodMaxCount, uma_suffix_);
  // We already reported ANR count on start for this session.
  log_on_start_pending_ = false;
}

void ArcMetricsAnr::UpdateRate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::TimeDelta duration =
      prefs_->GetTimeDelta(prefs::kAnrPendingDuration) + kUpdateInterval;
  if (duration >= kRateInterval) {
    duration = base::TimeDelta();
    RecordUmaWithSuffix(kRegularPeriodHistogram,
                        prefs_->GetInteger(prefs::kAnrPendingCount),
                        kForPeriodMaxCount, uma_suffix_);
    prefs_->SetInteger(prefs::kAnrPendingCount, 0);
  }

  prefs_->SetTimeDelta(prefs::kAnrPendingDuration, duration);
}

void ArcMetricsAnr::SetLogOnStartPending() {
  log_on_start_pending_ = true;
}

}  // namespace arc

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

namespace {

constexpr char kInvokeResultHistogramName[] = "Glic.InvokeResult";
constexpr char kInvokeSourceHistogramName[] = "Glic.Invoke.InvocationSource";
constexpr char kInvokeDurationHistogramName[] = "Glic.Invoke.Duration";

}  // namespace

GlicInvokeMetrics::GlicInvokeMetrics(mojom::InvocationSource source)
    : source_(source), invoke_start_time_(base::TimeTicks::Now()) {
  base::UmaHistogramEnumeration(kInvokeSourceHistogramName, source_);
}

void GlicInvokeMetrics::RecordSuccess() const {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName,
                                GlicInvokeResult::kSuccess);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source_)),
      GlicInvokeResult::kSuccess);

  base::TimeDelta duration = base::TimeTicks::Now() - invoke_start_time_;
  base::UmaHistogramLongTimes100(kInvokeDurationHistogramName, duration);
  base::UmaHistogramLongTimes100(
      base::StringPrintf("%s.%s", kInvokeDurationHistogramName,
                         GetInvocationSourceString(source_)),
      duration);
}

void GlicInvokeMetrics::RecordError(GlicInvokeError result) const {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName, result);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source_)),
      result);

  base::TimeDelta duration = base::TimeTicks::Now() - invoke_start_time_;
  base::UmaHistogramLongTimes100(kInvokeDurationHistogramName, duration);
  base::UmaHistogramLongTimes100(
      base::StringPrintf("%s.%s", kInvokeDurationHistogramName,
                         GetInvocationSourceString(source_)),
      duration);
}

}  // namespace glic

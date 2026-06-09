// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

namespace {

constexpr char kHistogramName[] = "Glic.InvokeResult";

}  // namespace

void RecordInvokeSuccess(mojom::InvocationSource source) {
  base::UmaHistogramEnumeration(kHistogramName, GlicInvokeResult::kSuccess);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kHistogramName,
                         GetInvocationSourceString(source)),
      GlicInvokeResult::kSuccess);
}

void RecordInvokeError(mojom::InvocationSource source, GlicInvokeError result) {
  base::UmaHistogramEnumeration(kHistogramName, result);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kHistogramName,
                         GetInvocationSourceString(source)),
      result);
}

}  // namespace glic

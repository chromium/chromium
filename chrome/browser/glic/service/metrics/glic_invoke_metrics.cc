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

}  // namespace

void RecordInvokeSource(mojom::InvocationSource source) {
  base::UmaHistogramEnumeration(kInvokeSourceHistogramName, source);
}

void RecordInvokeSuccess(mojom::InvocationSource source) {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName,
                                GlicInvokeResult::kSuccess);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source)),
      GlicInvokeResult::kSuccess);
}

void RecordInvokeError(mojom::InvocationSource source, GlicInvokeError result) {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName, result);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source)),
      result);
}

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

// This enum works around needing to add a kSuccess to GlicInvokeError solely
// for metrics, which would add confusion. The `0` entry of GlicInvokeError is
// unused so that these enums can be "overlaid".
enum class GlicInvokeResult {
  kSuccess = 0,
  kErrorMaxValue = static_cast<int>(GlicInvokeError::kMaxValue),
  kMaxValue = kErrorMaxValue
};

class GlicInvokeMetrics {
 public:
  explicit GlicInvokeMetrics(mojom::InvocationSource source);
  ~GlicInvokeMetrics() = default;

  GlicInvokeMetrics(const GlicInvokeMetrics&) = delete;
  GlicInvokeMetrics& operator=(const GlicInvokeMetrics&) = delete;

  void RecordSuccess() const;
  void RecordError(GlicInvokeError result) const;

 private:
  mojom::InvocationSource source_;
  base::TimeTicks invoke_start_time_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_

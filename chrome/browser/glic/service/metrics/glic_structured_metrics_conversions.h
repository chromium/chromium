// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_STRUCTURED_METRICS_CONVERSIONS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_STRUCTURED_METRICS_CONVERSIONS_H_

#include "chrome/browser/glic/glic_enums.h"
#include "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/structured_events.h"

namespace glic {

// Translates `ClientLoadErrorReason` into the structured metrics enum generated
// from structured.xml.
metrics::structured::events::v2::glic::GlicClientLoadErrorReason
ToStructuredClientLoadErrorReason(ClientLoadErrorReason reason);

}  // namespace glic
#endif  // BUILDFLAG(STRUCTURED_METRICS_ENABLED)

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_STRUCTURED_METRICS_CONVERSIONS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_

#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

void RecordInvokeSuccess(mojom::InvocationSource source);

void RecordInvokeError(mojom::InvocationSource source, GlicInvokeError result);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_

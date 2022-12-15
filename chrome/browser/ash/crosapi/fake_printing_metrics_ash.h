// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_PRINTING_METRICS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_PRINTING_METRICS_ASH_H_

#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS)
#error Fake PrintingMetricsAsh cannot be used with the USE_CUPS flag.
#endif

namespace crosapi {

// This class is a dummy counterpart for the real
// PrintingMetricsAsh declared in printing_metrics_ash.h to make
// std::unique_ptr<PrintingMetricsAsh> member in CrosapiAsh
// destructible even when USE_CUPS flag is not defined and the
// header printing_metrics_ash.h is not included.
class PrintingMetricsAsh {};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_PRINTING_METRICS_ASH_H_

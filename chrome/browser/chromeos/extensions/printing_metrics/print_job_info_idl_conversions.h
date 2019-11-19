// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_INFO_IDL_CONVERSIONS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_INFO_IDL_CONVERSIONS_H_

#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/common/extensions/api/printing_metrics.h"

namespace extensions {

api::printing_metrics::PrintJobInfo PrintJobInfoProtoToIdl(
    const chromeos::printing::proto::PrintJobInfo& print_job_info_proto);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_INFO_IDL_CONVERSIONS_H_

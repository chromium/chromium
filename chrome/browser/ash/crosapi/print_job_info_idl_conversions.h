// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PRINT_JOB_INFO_IDL_CONVERSIONS_H_
#define CHROME_BROWSER_ASH_CROSAPI_PRINT_JOB_INFO_IDL_CONVERSIONS_H_

#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/common/extensions/api/printing_metrics.h"

namespace extensions {

api::printing_metrics::PrintJobInfo PrintJobInfoProtoToIdl(
    const ash::printing::proto::PrintJobInfo& print_job_info_proto);

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_CROSAPI_PRINT_JOB_INFO_IDL_CONVERSIONS_H_

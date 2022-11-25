// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_INFO_PROTO_CONVERSIONS_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_INFO_PROTO_CONVERSIONS_H_

#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "printing/print_settings.h"

namespace ash {

// This conversion is used to store only necessary part of huge PrintSettings
// object in proto format. This proto is later used for saving print job
// history.
printing::proto::PrintSettings PrintSettingsToProto(
    const ::printing::PrintSettings& settings);

printing::proto::PrintJobInfo CupsPrintJobToProto(
    const CupsPrintJob& print_job,
    const std::string& id,
    const base::Time& completion_time);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_INFO_PROTO_CONVERSIONS_H_

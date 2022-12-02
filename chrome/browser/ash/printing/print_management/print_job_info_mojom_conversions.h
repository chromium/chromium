// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_JOB_INFO_MOJOM_CONVERSIONS_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_JOB_INFO_MOJOM_CONVERSIONS_H_

#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"

namespace ash {

class CupsPrintJob;

namespace printing {
namespace proto {
class PrintJobInfo;
}  // namespace proto

namespace print_management {

// Converts proto::PrintJobInfo into mojom::PrintJobInfoPtr.
chromeos::printing::printing_manager::mojom::PrintJobInfoPtr
PrintJobProtoToMojom(const proto::PrintJobInfo& print_job_info_proto);

// Convert CupsPrintJob into mojom::PrintJobInfoPtr.
chromeos::printing::printing_manager::mojom::PrintJobInfoPtr
CupsPrintJobToMojom(const CupsPrintJob& job);

}  // namespace print_management
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_JOB_INFO_MOJOM_CONVERSIONS_H_

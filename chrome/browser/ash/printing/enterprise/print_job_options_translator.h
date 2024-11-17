// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_JOB_OPTIONS_TRANSLATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_JOB_OPTIONS_TRANSLATOR_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/print_job_options.pb.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// Parses `print_job_options` field of the managed printer configuration JSON.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
PrintJobOptions ManagedPrintOptionsProtoFromDict(
    const base::Value::Dict& print_job_options);

// Converts PrintJobOptions proto to ChromeOS print job options representation.
// Returns std::nullopt if the proto is malformed.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
std::optional<Printer::ManagedPrintOptions>
ChromeOsPrintOptionsFromManagedPrintOptions(
    const PrintJobOptions& print_job_options);

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_JOB_OPTIONS_TRANSLATOR_H_

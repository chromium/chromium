// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_UMA_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_UMA_H_

#include <string>

#include "base/metrics/histogram.h"
#include "components/user_data_importer/common/importer_type.h"

namespace importer {

// Logs to UMA that an Importer of the specified |type| was used. Uses
// |metric_postfix| to split by entry point. Note: Values passed via
// |metric_postfix| require a matching "Import.ImporterType.|metric_postfix|"
// entry in tools/metrics/histograms/histograms.xml.
void LogImporterUseToMetrics(const std::string& metric_prefix,
                             user_data_importer::ImporterType type);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_UMA_H_

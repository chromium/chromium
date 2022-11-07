// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_UTILS_H_
#define CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_UTILS_H_

#include "chrome/browser/support_tool/data_collector.h"

void MergePIIMaps(PIIMap& out_pii_map, const PIIMap& from_pii_map);

#endif  // CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_UTILS_H_

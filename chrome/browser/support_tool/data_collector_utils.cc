// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/data_collector_utils.h"

#include <map>
#include <set>

#include "chrome/browser/support_tool/data_collector.h"

void MergePIIMaps(PIIMap& out_pii_map, const PIIMap& from_pii_map) {
  for (auto& pii_data : from_pii_map) {
    out_pii_map[pii_data.first].insert(pii_data.second.begin(),
                                       pii_data.second.end());
  }
}

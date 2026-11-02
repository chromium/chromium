// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DISCARDS_TAB_DISCARDS_DUMP_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DISCARDS_TAB_DISCARDS_DUMP_ANDROID_H_

#include <string>

#include "components/performance_manager/public/graph/graph.h"

namespace discards {

// Formats per-tab discardability diagnostics (mirroring chrome://discards,
// without privacy-sensitive URLs or titles) for all tab PageNodes in `graph`.
std::string DumpTabDiscardsInfo(performance_manager::Graph* graph);

}  // namespace discards

#endif  // CHROME_BROWSER_ANDROID_DISCARDS_TAB_DISCARDS_DUMP_ANDROID_H_

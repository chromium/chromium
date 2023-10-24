// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_METRICS_H_
#define CHROME_BROWSER_SHARE_SHARE_METRICS_H_

#include "base/time/time.h"

namespace share {

// The source from which the sharing hub was launched from.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with ShareSourceDesktop
// in src/tools/metrics/histograms/enums.xml.
enum class ShareSourceDesktop {
  kUnknown = 0,
  kOmniboxSharingHub = 1,
  // kWebContextMenu = 2,
  kAppMenuSharingHub = 3,
  kMaxValue = kAppMenuSharingHub,
};

void LogShareSourceDesktop(ShareSourceDesktop source);

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_METRICS_H_
